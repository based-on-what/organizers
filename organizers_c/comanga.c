/*
 * comanga.c
 * Translated from: comanga.py
 *
 * Purpose: Comic and Manga Page Counter.
 *   Counts pages (images) in CBZ, CBR, EPUB, and PDF files.
 *   Recursively scans a directory; subdirectories are aggregated as
 *   series/volumes.  Results are printed to stdout and saved to
 *   "page_count_results.txt" in the current working directory.
 *
 * Windows-native APIs used:
 *   - FindFirstFile / FindNextFile  (directory traversal)
 *   - CreateFile / GetFileSizeEx    (file I/O)
 *   - ZIP central-directory parser  (CBZ and EPUB — both are ZIP files)
 *   - Binary scan of PDF byte stream to count page objects
 *   - ShellExecuteEx / CreateProcess for unrar.exe (CBR fallback)
 *
 * Limitations:
 *   - CBR (RAR) support requires unrar.exe to be present in PATH.
 *     Without it, CBR files report 0 pages and log a warning.
 *   - PDF page counting uses a heuristic byte-scan ("/Type /Page")
 *     which works for virtually all standard PDFs but may
 *     miscount heavily compressed / cross-reference-stream PDFs.
 *
 * Compilation (MSVC):
 *   cl /W3 /std:c11 comanga.c shared_utils.c /Fe:comanga.exe
 * Compilation (MinGW):
 *   gcc -std=c11 -Wall comanga.c shared_utils.c -o comanga.exe
 */

#include "shared_utils.h"
#include <stdarg.h>

/* ------------------------------------------------------------------ */
/*  Image extensions recognised inside CBZ / CBR archives             */
/* ------------------------------------------------------------------ */
static const char *IMAGE_EXTS[] = {
    ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".webp"
};
#define IMAGE_EXTS_COUNT 6

static int is_image_file(const char *filename)
{
    for (int i = 0; i < IMAGE_EXTS_COUNT; i++)
        if (str_ends_with_ci(filename, IMAGE_EXTS[i]))
            return 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  PDF page counter (binary scan)                                      */
/* ------------------------------------------------------------------ */
/*
 * Scans the raw PDF bytes for occurrences of the pattern
 *   "/Type" <whitespace> "/Page"
 * followed by a non-alpha, non-digit character (so "/Pages" is not
 * counted).  This matches individual page-object dictionaries.
 */
static int count_pages_pdf(const char *pdf_path)
{
    if (!safe_file_operation(pdf_path))
        return -1;

    wchar_t wpdf[MAX_PATH];
    utf8_to_wide(pdf_path, wpdf, MAX_PATH);
    HANDLE hFile = CreateFileW(wpdf, GENERIC_READ, FILE_SHARE_READ,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        LOG_E("Cannot open PDF: %s", pdf_path);
        return -1;
    }

    LARGE_INTEGER fs;
    GetFileSizeEx(hFile, &fs);
    if (fs.QuadPart == 0 || fs.QuadPart > 200 * 1024 * 1024LL) {
        /* Skip files larger than 200 MB to avoid huge allocations */
        CloseHandle(hFile);
        LOG_W("PDF too large or empty, skipping: %s", pdf_path);
        return -1;
    }

    DWORD file_size = (DWORD)fs.QuadPart;
    unsigned char *buf = (unsigned char *)malloc(file_size);
    if (!buf) { CloseHandle(hFile); return -1; }

    DWORD bytes_read = 0;
    ReadFile(hFile, buf, file_size, &bytes_read, NULL);
    CloseHandle(hFile);

    int page_count = 0;
    /* Search for "/Type" then optional whitespace then "/Page"
       then a non-'s' character (to skip "/Pages").              */
    for (DWORD i = 0; i + 12 < bytes_read; i++) {
        if (buf[i] != '/') continue;
        if (_strnicmp((char *)buf + i, "/Type", 5) != 0) continue;

        /* Skip whitespace */
        DWORD j = i + 5;
        while (j < bytes_read && (buf[j] == ' ' || buf[j] == '\t' ||
               buf[j] == '\r' || buf[j] == '\n'))
            j++;

        if (j + 5 >= bytes_read) continue;
        if (_strnicmp((char *)buf + j, "/Page", 5) != 0) continue;

        /* The character after "/Page" must NOT be 's' (that would be /Pages) */
        char next = (char)buf[j + 5];
        if (tolower((unsigned char)next) == 's') continue;

        page_count++;
        i = j + 5;  /* advance past the match */
    }

    free(buf);
    return page_count;
}

/* ------------------------------------------------------------------ */
/*  CBZ page counter (ZIP-based)                                        */
/* ------------------------------------------------------------------ */
static int count_pages_cbz(const char *cbz_path)
{
    ZipFileList zfl;
    if (!zip_list_files(cbz_path, &zfl)) {
        LOG_E("Cannot read CBZ archive: %s", cbz_path);
        return -1;
    }

    int count = 0;
    for (int i = 0; i < zfl.count; i++)
        if (is_image_file(zfl.entries[i].filename))
            count++;

    zipfilelist_free(&zfl);
    return count;
}

/* ------------------------------------------------------------------ */
/*  EPUB page counter (ZIP-based: count HTML documents)                */
/* ------------------------------------------------------------------ */
static int count_pages_epub(const char *epub_path)
{
    ZipFileList zfl;
    if (!zip_list_files(epub_path, &zfl)) {
        LOG_E("Cannot read EPUB archive: %s", epub_path);
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
    return count;
}

/* ------------------------------------------------------------------ */
/*  CBR page counter (delegates to unrar.exe if available)             */
/* ------------------------------------------------------------------ */
/*
 * Runs: unrar.exe l -c- "<cbr_path>"
 * and counts lines that end with image extensions.
 * Returns -1 if unrar.exe is not found or fails.
 */
static int count_pages_cbr(const char *cbr_path)
{
    /* Build a command that lists archive contents */
    char cmd[MAX_PATH * 2 + 64];
    _snprintf(cmd, sizeof(cmd), "unrar.exe l -c- \"%s\"", cbr_path);

    /* Use _popen to capture output */
    FILE *pipe = _popen(cmd, "r");
    if (!pipe) {
        LOG_W("unrar.exe not found in PATH. CBR page count unavailable for: %s", cbr_path);
        return -1;
    }

    int count = 0;
    char line[1024];
    while (fgets(line, sizeof(line), pipe)) {
        /* Strip newline */
        line[strcspn(line, "\r\n")] = '\0';
        if (is_image_file(line))
            count++;
    }
    _pclose(pipe);
    return count;
}

/* ------------------------------------------------------------------ */
/*  Dispatch by extension                                               */
/* ------------------------------------------------------------------ */
static int count_pages_in_file(const char *path)
{
    if (str_ends_with_ci(path, ".cbz")) return count_pages_cbz(path);
    if (str_ends_with_ci(path, ".cbr")) return count_pages_cbr(path);
    if (str_ends_with_ci(path, ".epub"))return count_pages_epub(path);
    if (str_ends_with_ci(path, ".pdf")) return count_pages_pdf(path);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Result record (name + page count)                                  */
/* ------------------------------------------------------------------ */
typedef struct {
    char  name[MAX_PATH];
    int   pages;
} Result;

static int result_cmp(const void *a, const void *b)
{
    return ((const Result *)a)->pages - ((const Result *)b)->pages;
}

/* ------------------------------------------------------------------ */
/*  Directory analysis                                                  */
/* ------------------------------------------------------------------ */
static const char *COMIC_EXTS[]   = { ".cbz", ".cbr", ".epub", ".pdf" };
static const int   COMIC_EXT_CNT  = 4;

static Result  g_results[4096];
static int     g_result_count = 0;

static void process_file(const char *path, const char *display_name)
{
    if (!safe_file_operation(path)) {
        LOG_W("Skipping inaccessible file: %s", path);
        if (g_result_count < 4096) {
            _snprintf(g_results[g_result_count].name,
                      sizeof(g_results[0].name), "%s", display_name);
            g_results[g_result_count].pages = 0;
            g_result_count++;
        }
        return;
    }

    int pages = count_pages_in_file(path);
    if (pages < 0) {
        LOG_E("Error processing: %s", display_name);
        pages = 0;
    } else {
        LOG_I("  %s: %d pages", display_name, pages);
    }

    if (g_result_count < 4096) {
        _snprintf(g_results[g_result_count].name,
                  sizeof(g_results[0].name), "%s", display_name);
        g_results[g_result_count].pages = pages;
        g_result_count++;
    }
}

static void analyze_directory(const char *dir_path)
{
    wchar_t wdir[MAX_PATH], wpattern[MAX_PATH];
    utf8_to_wide(dir_path, wdir, MAX_PATH);
    _snwprintf(wpattern, MAX_PATH, L"%s\\*", wdir);

    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(wpattern, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        LOG_E("Cannot open directory: %s", dir_path);
        return;
    }

    LOG_I("Analyzing directory: %s", dir_path);
    LOG_I("--------------------------------------------------");

    do {
        if (wcscmp(ffd.cFileName, L".") == 0 ||
            wcscmp(ffd.cFileName, L"..") == 0)
            continue;

        char fname_u8[MAX_PATH * 3];
        wide_to_utf8(ffd.cFileName, fname_u8, sizeof(fname_u8));

        char full_path[MAX_PATH * 3];
        _snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, fname_u8);

        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            /* Aggregate subdirectory: find all comic files recursively */
            FileList *fl = filelist_create();
            find_files_by_extensions(full_path,
                                     COMIC_EXTS, COMIC_EXT_CNT,
                                     NULL, 0, 1, fl);

            if (fl->count > 0) {
                ProgressReporter pr;
                progress_init(&pr, fl->count, fname_u8);

                int total_pages = 0;
                for (int i = 0; i < fl->count; i++) {
                    if (safe_file_operation(fl->paths[i])) {
                        int p = count_pages_in_file(fl->paths[i]);
                        if (p > 0) total_pages += p;
                    }
                    progress_update(&pr, 1);
                }
                progress_finish(&pr);

                if (g_result_count < 4096) {
                    _snprintf(g_results[g_result_count].name,
                              sizeof(g_results[0].name), "%s", fname_u8);
                    g_results[g_result_count].pages = total_pages;
                    g_result_count++;
                }
                LOG_I("[DIR] %s: %d files, %d pages",
                      fname_u8, fl->count, total_pages);
            }
            filelist_free(fl);

        } else {
            /* Individual file in the root directory */
            int is_comic = 0;
            for (int i = 0; i < COMIC_EXT_CNT; i++)
                if (str_ends_with_ci(fname_u8, COMIC_EXTS[i])) {
                    is_comic = 1; break;
                }
            if (is_comic)
                process_file(full_path, fname_u8);
        }

    } while (FindNextFileW(hFind, &ffd));

    FindClose(hFind);
}

/* ------------------------------------------------------------------ */
/*  Display results and save to file                                    */
/* ------------------------------------------------------------------ */
static void display_results(void)
{
    if (g_result_count == 0) {
        LOG_I("No supported files found.");
        return;
    }

    /* Sort by page count ascending */
    qsort(g_results, g_result_count, sizeof(Result), result_cmp);

    LOG_I("\n==================================================");
    LOG_I("FINAL RESULTS (sorted by page count)");
    LOG_I("==================================================");

    int total_pages = 0;
    for (int i = 0; i < g_result_count; i++) {
        LOG_I("%s: %d pages", g_results[i].name, g_results[i].pages);
        total_pages += g_results[i].pages;
    }
    LOG_I("--------------------------------------------------");
    LOG_I("Total items: %d", g_result_count);
    LOG_I("Total pages: %d", total_pages);

    /* Build lines array for file output */
    char  summary1[64], summary2[64];
    _snprintf(summary1, sizeof(summary1), "Total items: %d", g_result_count);
    _snprintf(summary2, sizeof(summary2), "Total pages: %d", total_pages);

    int   line_count = g_result_count + 3;
    char **lines = (char **)malloc(line_count * sizeof(char *));
    char  *line_bufs = (char *)malloc(line_count * 256);
    if (!lines || !line_bufs) {
        free(lines); free(line_bufs);
        return;
    }

    for (int i = 0; i < g_result_count; i++) {
        lines[i] = line_bufs + i * 256;
        _snprintf(lines[i], 256, "%s: %d pages",
                  g_results[i].name, g_results[i].pages);
    }
    lines[g_result_count]     = line_bufs + g_result_count * 256;
    lines[g_result_count + 1] = line_bufs + (g_result_count + 1) * 256;
    lines[g_result_count + 2] = line_bufs + (g_result_count + 2) * 256;
    strcpy(lines[g_result_count],     "");
    strcpy(lines[g_result_count + 1], summary1);
    strcpy(lines[g_result_count + 2], summary2);

    save_results_to_file((const char **)lines, line_count,
                         "page_count_results.txt",
                         "COMIC/MANGA PAGE COUNT RESULTS");
    free(lines);
    free(line_bufs);
}

/* ------------------------------------------------------------------ */
/*  Entry point                                                         */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    log_init(LOG_INFO, NULL);

    char dir[MAX_PATH * 3];
    if (argc >= 2) {
        wchar_t warg[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, argv[1], -1, warg, MAX_PATH);
        wide_to_utf8(warg, dir, sizeof(dir));
    } else {
        /* Default: directory of this executable */
        wchar_t wdir_buf[MAX_PATH];
        GetModuleFileNameW(NULL, wdir_buf, MAX_PATH);
        /* Strip the filename to get the directory */
        wchar_t *last_sep = wcsrchr(wdir_buf, L'\\');
        if (last_sep) *last_sep = L'\0';
        wide_to_utf8(wdir_buf, dir, sizeof(dir));
    }

    LOG_I("Starting comic/manga analysis in: %s", dir);

    analyze_directory(dir);
    display_results();

    log_close();
    return 0;
}
