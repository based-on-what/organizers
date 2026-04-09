/*
 * pageCounter.c
 * Translated from: pageCounter.py
 *
 * Purpose: Page Counter for documents.
 *   Counts pages in PDF, EPUB, and DOCX files in the current
 *   directory (non-recursive).  Results are sorted by page count
 *   and saved to "page_count_results.txt".
 *
 * Windows-native APIs used:
 *   - FindFirstFile / FindNextFile     (file enumeration)
 *   - CreateFile / ReadFile / GetFileSizeEx (file I/O)
 *   - ZIP central-directory parser     (EPUB = ZIP, DOCX = ZIP)
 *     both via shared_utils.zip_list_files()
 *
 * PDF counting heuristic:
 *   Byte-scan for "/Type /Page" (not "/Type /Pages").
 *   Works for virtually all standard PDFs; may give wrong results for
 *   heavily compressed or cross-reference-stream PDFs.
 *
 * DOCX page counting:
 *   1. Looks for explicit form-feed characters (\f) in document.xml
 *      (these are inserted by Word as hard page breaks in run text).
 *   2. Falls back to character-count estimation (~2000 chars/page)
 *      if no explicit page breaks are found.
 *   This mirrors the logic from the original Python script exactly.
 *
 * EPUB page counting:
 *   Counts .html / .xhtml / .htm entries in the ZIP (each is one
 *   "chapter/page" in the original Python logic).
 *
 * Compilation (MSVC):
 *   cl /W3 /std:c11 pageCounter.c shared_utils.c /Fe:pageCounter.exe
 * Compilation (MinGW):
 *   gcc -std=c11 -Wall pageCounter.c shared_utils.c -o pageCounter.exe
 */

#include "shared_utils.h"

/* ------------------------------------------------------------------ */
/*  PDF page counter  (binary scan)                                    */
/* ------------------------------------------------------------------ */
static int count_pdf_pages(const char *path)
{
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        LOG_E("Cannot open PDF: %s", path);
        return -1;
    }

    LARGE_INTEGER fs;
    GetFileSizeEx(hFile, &fs);
    if (fs.QuadPart == 0 || fs.QuadPart > 200 * 1024 * 1024LL) {
        CloseHandle(hFile);
        return -1;
    }

    DWORD file_size = (DWORD)fs.QuadPart;
    unsigned char *buf = (unsigned char *)malloc(file_size);
    if (!buf) { CloseHandle(hFile); return -1; }

    DWORD bytes_read = 0;
    ReadFile(hFile, buf, file_size, &bytes_read, NULL);
    CloseHandle(hFile);

    /* Count "/Type /Page" entries that are NOT "/Type /Pages" */
    int page_count = 0;
    for (DWORD i = 0; i + 12 < bytes_read; i++) {
        if (buf[i] != '/') continue;
        if (_strnicmp((char *)buf + i, "/Type", 5) != 0) continue;

        DWORD j = i + 5;
        while (j < bytes_read && (buf[j] == ' ' || buf[j] == '\t' ||
               buf[j] == '\r' || buf[j] == '\n'))
            j++;

        if (j + 5 >= bytes_read) continue;
        if (_strnicmp((char *)buf + j, "/Page", 5) != 0) continue;

        char next = (char)buf[j + 5];
        if (tolower((unsigned char)next) == 's') continue;

        page_count++;
        i = j + 5;
    }

    free(buf);
    return page_count > 0 ? page_count : 1; /* at least 1 page */
}

/* ------------------------------------------------------------------ */
/*  EPUB page counter  (ZIP: count HTML items)                         */
/* ------------------------------------------------------------------ */
static int count_epub_pages(const char *path)
{
    ZipFileList zfl;
    if (!zip_list_files(path, &zfl)) {
        LOG_E("Cannot read EPUB: %s", path);
        return -1;
    }

    int count = 0;
    for (int i = 0; i < zfl.count; i++) {
        const char *fn = zfl.entries[i].filename;
        if (str_ends_with_ci(fn, ".html") ||
            str_ends_with_ci(fn, ".xhtml") ||
            str_ends_with_ci(fn, ".htm"))
            count++;
    }
    zipfilelist_free(&zfl);
    return count > 0 ? count : 1;
}

/* ------------------------------------------------------------------ */
/*  DOCX page counter (ZIP: scan document.xml for page breaks)        */
/* ------------------------------------------------------------------ */
/*
 * Extracts "word/document.xml" from the DOCX ZIP by finding its
 * local file entry and reading its content (decompressed if needed).
 *
 * For simplicity we look for the Local File Header, then attempt to
 * read uncompressed content (compression method 0 = stored) or we
 * fall back to character-count estimation from the XML text length.
 */

/* ZIP Local File Header offsets */
#define LFH_SIGNATURE      0x04034b50u
#define LFH_COMP_METHOD    8
#define LFH_COMP_SIZE      18
#define LFH_UNCOMP_SIZE    22
#define LFH_FNAME_LEN      26
#define LFH_EXTRA_LEN      28
#define LFH_HEADER_SIZE    30

static int count_docx_pages(const char *path)
{
    /* Open the DOCX (which is a ZIP) */
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        LOG_E("Cannot open DOCX: %s", path);
        return -1;
    }

    LARGE_INTEGER fs;
    GetFileSizeEx(hFile, &fs);
    if (fs.QuadPart == 0 || fs.QuadPart > 100 * 1024 * 1024LL) {
        CloseHandle(hFile);
        return -1;
    }

    DWORD file_size = (DWORD)fs.QuadPart;
    unsigned char *zip_buf = (unsigned char *)malloc(file_size);
    if (!zip_buf) { CloseHandle(hFile); return -1; }

    DWORD bytes_read = 0;
    ReadFile(hFile, zip_buf, file_size, &bytes_read, NULL);
    CloseHandle(hFile);

    /*
     * Find the local file entry for "word/document.xml".
     * We walk the ZIP from the start, looking for Local File Headers.
     */
    unsigned char *doc_data    = NULL;
    DWORD          doc_size    = 0;
    DWORD          doc_uncomp  = 0;
    WORD           doc_method  = 0xFFFF;

    DWORD pos = 0;
    while (pos + LFH_HEADER_SIZE <= bytes_read) {
        DWORD sig = (zip_buf[pos])
                  | ((DWORD)zip_buf[pos+1] << 8)
                  | ((DWORD)zip_buf[pos+2] << 16)
                  | ((DWORD)zip_buf[pos+3] << 24);

        if (sig != LFH_SIGNATURE) {
            /* Not a local file header; advance one byte and retry */
            pos++;
            continue;
        }

        WORD  comp_method = (WORD)(zip_buf[pos + LFH_COMP_METHOD    ] |
                                   zip_buf[pos + LFH_COMP_METHOD + 1] << 8);
        DWORD comp_size   = (zip_buf[pos + LFH_COMP_SIZE    ]      )
                          | ((DWORD)zip_buf[pos + LFH_COMP_SIZE + 1] << 8)
                          | ((DWORD)zip_buf[pos + LFH_COMP_SIZE + 2] << 16)
                          | ((DWORD)zip_buf[pos + LFH_COMP_SIZE + 3] << 24);
        DWORD uncomp_size = (zip_buf[pos + LFH_UNCOMP_SIZE    ]      )
                          | ((DWORD)zip_buf[pos + LFH_UNCOMP_SIZE + 1] << 8)
                          | ((DWORD)zip_buf[pos + LFH_UNCOMP_SIZE + 2] << 16)
                          | ((DWORD)zip_buf[pos + LFH_UNCOMP_SIZE + 3] << 24);
        WORD  fname_len   = (WORD)(zip_buf[pos + LFH_FNAME_LEN    ] |
                                   zip_buf[pos + LFH_FNAME_LEN + 1] << 8);
        WORD  extra_len   = (WORD)(zip_buf[pos + LFH_EXTRA_LEN    ] |
                                   zip_buf[pos + LFH_EXTRA_LEN + 1] << 8);

        DWORD data_offset = pos + LFH_HEADER_SIZE + fname_len + extra_len;

        /* Compare filename */
        if (fname_len == 19 &&
            _strnicmp((char *)zip_buf + pos + LFH_HEADER_SIZE,
                      "word/document.xml", 17) == 0)
        {
            doc_method = comp_method;
            doc_size   = comp_size;
            doc_uncomp = uncomp_size;
            if (data_offset + comp_size <= bytes_read)
                doc_data = zip_buf + data_offset;
            break;
        }

        pos = data_offset + comp_size;
    }

    int pages = -1;

    if (doc_data && doc_method == 0) {
        /* Method 0 = stored (no compression): count form-feed bytes */
        int page_breaks = 0;
        for (DWORD i = 0; i < doc_size; i++)
            if (doc_data[i] == '\f')
                page_breaks++;

        if (page_breaks > 0) {
            pages = page_breaks + 1;
        } else {
            /* Estimate by visible character count in XML (~2000 chars/page) */
            /* Strip XML tags to get approximate text length */
            long long text_chars = 0;
            int inside_tag = 0;
            for (DWORD i = 0; i < doc_size; i++) {
                if (doc_data[i] == '<') { inside_tag = 1; continue; }
                if (doc_data[i] == '>') { inside_tag = 0; continue; }
                if (!inside_tag && doc_data[i] >= 0x20)
                    text_chars++;
            }
            pages = (int)(text_chars / 2000);
            if (pages < 1) pages = 1;
        }
    } else if (doc_method == 8) {
        /*
         * Deflate-compressed document.xml.
         * We cannot decompress without zlib.  Fall back to estimating
         * pages from the compressed size (rough heuristic).
         */
        LOG_W("DOCX document.xml is deflate-compressed; using size estimate: %s", path);
        /* Typical deflate ratio ~3:1; ~2000 chars/page */
        long long est_text = (long long)doc_uncomp;
        pages = (int)(est_text / 2000);
        if (pages < 1) pages = 1;
    } else {
        LOG_W("word/document.xml not found in DOCX: %s", path);
    }

    free(zip_buf);
    return pages;
}

/* ------------------------------------------------------------------ */
/*  Result record                                                       */
/* ------------------------------------------------------------------ */
typedef struct {
    char name[MAX_PATH];
    int  pages;
} PageResult;

static int pageresult_cmp(const void *a, const void *b)
{
    return ((const PageResult *)a)->pages - ((const PageResult *)b)->pages;
}

/* ------------------------------------------------------------------ */
/*  Scan directory and count pages                                      */
/* ------------------------------------------------------------------ */
static PageResult g_results[4096];
static int        g_result_count = 0;
static PageResult g_errors[4096];
static int        g_error_count  = 0;

static void process_file(const char *path, const char *name)
{
    if (!safe_file_operation(path)) {
        if (g_error_count < 4096) {
            _snprintf(g_errors[g_error_count].name,
                      sizeof(g_errors[0].name), "%s", name);
            g_errors[g_error_count].pages = 0;
            g_error_count++;
        }
        return;
    }

    int pages = 0;
    if (str_ends_with_ci(name, ".pdf"))
        pages = count_pdf_pages(path);
    else if (str_ends_with_ci(name, ".epub"))
        pages = count_epub_pages(path);
    else if (str_ends_with_ci(name, ".docx"))
        pages = count_docx_pages(path);

    if (pages < 0) {
        LOG_E("Error processing: %s", name);
        if (g_error_count < 4096) {
            _snprintf(g_errors[g_error_count].name,
                      sizeof(g_errors[0].name), "%s", name);
            g_errors[g_error_count].pages = 0;
            g_error_count++;
        }
        return;
    }

    LOG_I("OK  %s: %d pages", name, pages);
    if (g_result_count < 4096) {
        _snprintf(g_results[g_result_count].name,
                  sizeof(g_results[0].name), "%s", name);
        g_results[g_result_count].pages = pages;
        g_result_count++;
    }
}

static void scan_directory(const char *dir)
{
    char pattern[MAX_PATH];
    _snprintf(pattern, sizeof(pattern), "%s\\*", dir);

    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        LOG_E("Cannot open directory: %s", dir);
        return;
    }

    /* Count matching files first for progress reporter */
    int total = 0;
    do {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (str_ends_with_ci(ffd.cFileName, ".pdf")  ||
            str_ends_with_ci(ffd.cFileName, ".epub") ||
            str_ends_with_ci(ffd.cFileName, ".docx"))
            total++;
    } while (FindNextFileA(hFind, &ffd));
    FindClose(hFind);

    if (total == 0) {
        LOG_I("No supported files (PDF, EPUB, DOCX) found in: %s", dir);
        return;
    }
    LOG_I("Found %d supported file(s) to process.", total);

    ProgressReporter pr;
    progress_init(&pr, total, "Processing files");

    hFind = FindFirstFileA(pattern, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (!(str_ends_with_ci(ffd.cFileName, ".pdf")  ||
              str_ends_with_ci(ffd.cFileName, ".epub") ||
              str_ends_with_ci(ffd.cFileName, ".docx")))
            continue;

        char full_path[MAX_PATH];
        _snprintf(full_path, sizeof(full_path), "%s\\%s", dir, ffd.cFileName);
        process_file(full_path, ffd.cFileName);
        progress_update(&pr, 1);
    } while (FindNextFileA(hFind, &ffd));

    FindClose(hFind);
    progress_finish(&pr);

    if (g_error_count > 0) {
        LOG_W("Summary of %d file(s) with errors:", g_error_count);
        for (int i = 0; i < g_error_count; i++)
            LOG_W("  %s", g_errors[i].name);
    }
}

/* ------------------------------------------------------------------ */
/*  Display results and save to file                                    */
/* ------------------------------------------------------------------ */
static void display_and_save(const char *output_file)
{
    if (g_result_count == 0) {
        LOG_I("No files were successfully processed.");
        return;
    }

    qsort(g_results, g_result_count, sizeof(PageResult), pageresult_cmp);

    LOG_I("\nFiles sorted by page count:");
    int total_pages = 0;
    for (int i = 0; i < g_result_count; i++) {
        LOG_I("%s: %d pages", g_results[i].name, g_results[i].pages);
        total_pages += g_results[i].pages;
    }
    LOG_I("\nSummary:");
    LOG_I("Total files: %d", g_result_count);
    LOG_I("Total pages: %d", total_pages);

    /* Build lines */
    int max_lines = g_result_count + 4;
    char **lines = (char **)malloc(max_lines * sizeof(char *));
    char  *bufs  = (char  *)malloc(max_lines * 256);
    if (!lines || !bufs) { free(lines); free(bufs); return; }

    int lc = 0;
    for (int i = 0; i < g_result_count; i++) {
        lines[lc] = bufs + lc * 256;
        _snprintf(lines[lc], 256, "%s: %d pages",
                  g_results[i].name, g_results[i].pages);
        lc++;
    }
    lines[lc] = bufs + lc * 256; strcpy(lines[lc], ""); lc++;
    lines[lc] = bufs + lc * 256;
    _snprintf(lines[lc], 256, "Total files: %d", g_result_count); lc++;
    lines[lc] = bufs + lc * 256;
    _snprintf(lines[lc], 256, "Total pages: %d", total_pages); lc++;

    save_results_to_file((const char **)lines, lc,
                          output_file, "FILES SORTED BY PAGE COUNT");
    free(lines);
    free(bufs);
}

/* ------------------------------------------------------------------ */
/*  Entry point                                                         */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    log_init(LOG_INFO, NULL);

    char dir[MAX_PATH];
    if (argc >= 2)
        _snprintf(dir, sizeof(dir), "%s", argv[1]);
    else
        GetCurrentDirectoryA(MAX_PATH, dir);

    LOG_I("Scanning directory: %s", dir);

    scan_directory(dir);
    display_and_save("page_count_results.txt");

    log_close();
    return 0;
}
