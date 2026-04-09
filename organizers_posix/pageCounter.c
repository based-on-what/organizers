/*
 * pageCounter.c  (POSIX version)
 * Translated from: pageCounter.py
 *
 * Purpose: Page Counter for PDF, EPUB, and DOCX files.
 *
 * POSIX differences vs Windows version:
 *   - File I/O: fopen/fread/fseek instead of CreateFile/ReadFile.
 *   - Directory traversal: opendir/readdir instead of FindFirstFile.
 *   - Everything else (ZIP parser, PDF scan, DOCX logic) is identical
 *     since those operate purely on byte buffers.
 *
 * Compilation (Linux):
 *   gcc -std=c11 -Wall -O2 pageCounter.c shared_utils.c -o pageCounter
 * Compilation (macOS):
 *   clang -std=c11 -Wall -O2 pageCounter.c shared_utils.c -o pageCounter
 */

#include "shared_utils.h"

/* ------------------------------------------------------------------ */
/*  PDF page counter                                                    */
/* ------------------------------------------------------------------ */
static int count_pdf_pages(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) { LOG_E("Cannot open PDF: %s", path); return -1; }
    fseek(f, 0, SEEK_END);
    long fs = ftell(f); rewind(f);
    if (fs <= 0 || fs > 200 * 1024 * 1024L) { fclose(f); return -1; }

    unsigned char *buf = malloc((size_t)fs);
    if (!buf) { fclose(f); return -1; }
    size_t bytes_read = fread(buf, 1, (size_t)fs, f);
    fclose(f);

    int page_count = 0;
    for (size_t i = 0; i + 12 < bytes_read; i++) {
        if (buf[i] != '/') continue;
        if (strncasecmp((char *)buf+i, "/Type", 5) != 0) continue;
        size_t j = i + 5;
        while (j < bytes_read && (buf[j]==' '||buf[j]=='\t'||buf[j]=='\r'||buf[j]=='\n')) j++;
        if (j + 5 >= bytes_read) continue;
        if (strncasecmp((char *)buf+j, "/Page", 5) != 0) continue;
        if (tolower((unsigned char)buf[j+5]) == 's') continue;
        page_count++;
        i = j + 5;
    }
    free(buf);
    return page_count > 0 ? page_count : 1;
}

/* ------------------------------------------------------------------ */
/*  EPUB page counter                                                   */
/* ------------------------------------------------------------------ */
static int count_epub_pages(const char *path)
{
    ZipFileList zfl;
    if (!zip_list_files(path, &zfl)) {
        LOG_E("Cannot read EPUB: %s", path); return -1;
    }
    int count = 0;
    for (int i = 0; i < zfl.count; i++) {
        const char *fn = zfl.entries[i].filename;
        if (str_ends_with_ci(fn, ".html") || str_ends_with_ci(fn, ".xhtml") ||
            str_ends_with_ci(fn, ".htm"))
            count++;
    }
    zipfilelist_free(&zfl);
    return count > 0 ? count : 1;
}

/* ------------------------------------------------------------------ */
/*  DOCX page counter                                                   */
/* ------------------------------------------------------------------ */
#define LFH_SIGNATURE  0x04034b50u
#define LFH_COMP_METHOD 8
#define LFH_COMP_SIZE   18
#define LFH_UNCOMP_SIZE 22
#define LFH_FNAME_LEN   26
#define LFH_EXTRA_LEN   28
#define LFH_HEADER_SIZE 30

static unsigned int  r32(const unsigned char *p)
{ return (unsigned int)p[0]|((unsigned int)p[1]<<8)|((unsigned int)p[2]<<16)|((unsigned int)p[3]<<24); }
static unsigned short r16(const unsigned char *p)
{ return (unsigned short)(p[0]|(p[1]<<8)); }

static int count_docx_pages(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) { LOG_E("Cannot open DOCX: %s", path); return -1; }
    fseek(f, 0, SEEK_END);
    long fs = ftell(f); rewind(f);
    if (fs <= 0 || fs > 100 * 1024 * 1024L) { fclose(f); return -1; }

    unsigned char *zip_buf = malloc((size_t)fs);
    if (!zip_buf) { fclose(f); return -1; }
    size_t bytes_read = fread(zip_buf, 1, (size_t)fs, f);
    fclose(f);

    unsigned char *doc_data = NULL;
    unsigned int   doc_size = 0, doc_uncomp = 0;
    unsigned short doc_method = 0xFFFF;

    size_t pos = 0;
    while (pos + LFH_HEADER_SIZE <= bytes_read) {
        unsigned int sig = r32(zip_buf + pos);
        if (sig != LFH_SIGNATURE) { pos++; continue; }

        unsigned short comp_method = r16(zip_buf + pos + LFH_COMP_METHOD);
        unsigned int   comp_size   = r32(zip_buf + pos + LFH_COMP_SIZE);
        unsigned int   uncomp_size = r32(zip_buf + pos + LFH_UNCOMP_SIZE);
        unsigned short fname_len   = r16(zip_buf + pos + LFH_FNAME_LEN);
        unsigned short extra_len   = r16(zip_buf + pos + LFH_EXTRA_LEN);
        size_t data_offset = pos + LFH_HEADER_SIZE + fname_len + extra_len;

        if (fname_len == 19 &&
            strncasecmp((char *)zip_buf + pos + LFH_HEADER_SIZE,
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
        int page_breaks = 0;
        for (unsigned int i = 0; i < doc_size; i++)
            if (doc_data[i] == '\f') page_breaks++;
        if (page_breaks > 0) {
            pages = page_breaks + 1;
        } else {
            long long text_chars = 0;
            int inside_tag = 0;
            for (unsigned int i = 0; i < doc_size; i++) {
                if (doc_data[i] == '<') { inside_tag = 1; continue; }
                if (doc_data[i] == '>') { inside_tag = 0; continue; }
                if (!inside_tag && doc_data[i] >= 0x20) text_chars++;
            }
            pages = (int)(text_chars / 2000);
            if (pages < 1) pages = 1;
        }
    } else if (doc_method == 8) {
        LOG_W("DOCX document.xml is deflate-compressed; using size estimate: %s", path);
        pages = (int)(doc_uncomp / 2000);
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
typedef struct { char name[PATH_MAX]; int pages; } PageResult;
static int pr_cmp(const void *a, const void *b)
{ return ((const PageResult *)a)->pages - ((const PageResult *)b)->pages; }

static PageResult g_results[4096]; static int g_result_count = 0;
static PageResult g_errors[4096];  static int g_error_count  = 0;

static void process_file(const char *path, const char *name)
{
    if (!safe_file_operation(path)) {
        if (g_error_count < 4096) {
            snprintf(g_errors[g_error_count].name, PATH_MAX, "%s", name);
            g_errors[g_error_count++].pages = 0;
        }
        return;
    }
    int pages = 0;
    if (str_ends_with_ci(name, ".pdf"))  pages = count_pdf_pages(path);
    else if (str_ends_with_ci(name, ".epub")) pages = count_epub_pages(path);
    else if (str_ends_with_ci(name, ".docx")) pages = count_docx_pages(path);

    if (pages < 0) {
        LOG_E("Error processing: %s", name);
        if (g_error_count < 4096) {
            snprintf(g_errors[g_error_count].name, PATH_MAX, "%s", name);
            g_errors[g_error_count++].pages = 0;
        }
        return;
    }
    LOG_I("OK  %s: %d pages", name, pages);
    if (g_result_count < 4096) {
        snprintf(g_results[g_result_count].name, PATH_MAX, "%s", name);
        g_results[g_result_count++].pages = pages;
    }
}

static void scan_directory(const char *dir_path)
{
    DIR *dir = opendir(dir_path);
    if (!dir) { LOG_E("Cannot open directory: %s", dir_path); return; }

    /* Count first for progress reporter */
    int total = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (str_ends_with_ci(entry->d_name, ".pdf")  ||
            str_ends_with_ci(entry->d_name, ".epub") ||
            str_ends_with_ci(entry->d_name, ".docx"))
            total++;
    }
    rewinddir(dir);

    if (total == 0) {
        LOG_I("No supported files (PDF, EPUB, DOCX) found in: %s", dir_path);
        closedir(dir); return;
    }
    LOG_I("Found %d supported file(s).", total);

    ProgressReporter pr;
    progress_init(&pr, total, "Processing files");

    while ((entry = readdir(dir)) != NULL) {
        if (!(str_ends_with_ci(entry->d_name, ".pdf")  ||
              str_ends_with_ci(entry->d_name, ".epub") ||
              str_ends_with_ci(entry->d_name, ".docx"))) continue;

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        struct stat st;
        if (stat(full_path, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        process_file(full_path, entry->d_name);
        progress_update(&pr, 1);
    }
    closedir(dir);
    progress_finish(&pr);

    if (g_error_count > 0) {
        LOG_W("Files with errors: %d", g_error_count);
        for (int i = 0; i < g_error_count; i++)
            LOG_W("  %s", g_errors[i].name);
    }
}

static void display_and_save(const char *output_file)
{
    if (g_result_count == 0) { LOG_I("No files processed."); return; }
    qsort(g_results, g_result_count, sizeof(PageResult), pr_cmp);

    int total_pages = 0;
    for (int i = 0; i < g_result_count; i++) total_pages += g_results[i].pages;

    int    max_lines = g_result_count + 4;
    char **lines = malloc(max_lines * sizeof(char *));
    char  *bufs  = malloc(max_lines * 256);
    if (!lines || !bufs) { free(lines); free(bufs); return; }
    int lc = 0;
    for (int i = 0; i < g_result_count; i++) {
        lines[lc] = bufs + lc * 256;
        snprintf(lines[lc], 256, "%s: %d pages", g_results[i].name, g_results[i].pages);
        lc++;
    }
    lines[lc] = bufs + lc*256; strcpy(lines[lc], ""); lc++;
    lines[lc] = bufs + lc*256; snprintf(lines[lc],256,"Total files: %d",g_result_count); lc++;
    lines[lc] = bufs + lc*256; snprintf(lines[lc],256,"Total pages: %d",total_pages);    lc++;
    save_results_to_file((const char **)lines, lc, output_file, "FILES SORTED BY PAGE COUNT");
    free(lines); free(bufs);
}

int main(int argc, char *argv[])
{
    log_init(LOG_INFO, NULL);
    char dir[PATH_MAX];
    if (argc >= 2) snprintf(dir, sizeof(dir), "%s", argv[1]);
    else           getcwd(dir, sizeof(dir));
    LOG_I("Scanning directory: %s", dir);
    scan_directory(dir);
    display_and_save("page_count_results.txt");
    log_close();
    return 0;
}
