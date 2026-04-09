/*
 * comanga.c  (POSIX version)
 * Translated from: comanga.py
 *
 * Purpose: Comic and Manga Page Counter.
 *   Counts pages in CBZ, CBR, EPUB, and PDF files.
 *
 * POSIX differences vs Windows version:
 *   - Directory traversal: opendir/readdir (dirent.h) instead of FindFirstFile
 *   - File I/O: fopen/fread instead of CreateFile/ReadFile
 *   - Path separator: '/' instead of '\'
 *   - CBR: popen("unrar") — same approach, popen is POSIX standard
 *
 * Compilation (Linux):
 *   gcc -std=c11 -Wall -O2 comanga.c shared_utils.c -o comanga
 * Compilation (macOS):
 *   clang -std=c11 -Wall -O2 comanga.c shared_utils.c -o comanga
 */

#include "shared_utils.h"

static const char *IMAGE_EXTS[]  = { ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".webp" };
#define IMAGE_EXTS_COUNT 6

static int is_image_file(const char *filename)
{
    for (int i = 0; i < IMAGE_EXTS_COUNT; i++)
        if (str_ends_with_ci(filename, IMAGE_EXTS[i])) return 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  PDF page counter (binary scan — identical logic to Windows)        */
/* ------------------------------------------------------------------ */
static int count_pages_pdf(const char *pdf_path)
{
    if (!safe_file_operation(pdf_path)) return -1;

    FILE *f = fopen(pdf_path, "rb");
    if (!f) { LOG_E("Cannot open PDF: %s", pdf_path); return -1; }

    fseek(f, 0, SEEK_END);
    long fs = ftell(f);
    rewind(f);
    if (fs <= 0 || fs > 200 * 1024 * 1024L) { fclose(f); return -1; }

    unsigned char *buf = malloc((size_t)fs);
    if (!buf) { fclose(f); return -1; }
    size_t bytes_read = fread(buf, 1, (size_t)fs, f);
    fclose(f);

    int page_count = 0;
    for (size_t i = 0; i + 12 < bytes_read; i++) {
        if (buf[i] != '/') continue;
        if (strncasecmp((char *)buf + i, "/Type", 5) != 0) continue;
        size_t j = i + 5;
        while (j < bytes_read && (buf[j]==' '||buf[j]=='\t'||buf[j]=='\r'||buf[j]=='\n')) j++;
        if (j + 5 >= bytes_read) continue;
        if (strncasecmp((char *)buf + j, "/Page", 5) != 0) continue;
        if (tolower((unsigned char)buf[j+5]) == 's') continue;
        page_count++;
        i = j + 5;
    }
    free(buf);
    return page_count > 0 ? page_count : 1;
}

/* ------------------------------------------------------------------ */
/*  CBZ page counter (ZIP-based)                                        */
/* ------------------------------------------------------------------ */
static int count_pages_cbz(const char *cbz_path)
{
    ZipFileList zfl;
    if (!zip_list_files(cbz_path, &zfl)) {
        LOG_E("Cannot read CBZ: %s", cbz_path); return -1;
    }
    int count = 0;
    for (int i = 0; i < zfl.count; i++)
        if (is_image_file(zfl.entries[i].filename)) count++;
    zipfilelist_free(&zfl);
    return count;
}

/* ------------------------------------------------------------------ */
/*  EPUB page counter (ZIP-based)                                       */
/* ------------------------------------------------------------------ */
static int count_pages_epub(const char *epub_path)
{
    ZipFileList zfl;
    if (!zip_list_files(epub_path, &zfl)) {
        LOG_E("Cannot read EPUB: %s", epub_path); return -1;
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
/*  CBR page counter (popen unrar — POSIX standard)                    */
/* ------------------------------------------------------------------ */
static int count_pages_cbr(const char *cbr_path)
{
    char cmd[PATH_MAX * 2 + 32];
    snprintf(cmd, sizeof(cmd), "unrar l -c- \"%s\" 2>/dev/null", cbr_path);
    FILE *pipe = popen(cmd, "r");
    if (!pipe) {
        LOG_W("unrar not found in PATH. CBR page count unavailable: %s", cbr_path);
        return -1;
    }
    int count = 0;
    char line[1024];
    while (fgets(line, sizeof(line), pipe)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (is_image_file(line)) count++;
    }
    pclose(pipe);
    return count;
}

/* ------------------------------------------------------------------ */
/*  Dispatch by extension                                               */
/* ------------------------------------------------------------------ */
static int count_pages_in_file(const char *path)
{
    if (str_ends_with_ci(path, ".cbz"))  return count_pages_cbz(path);
    if (str_ends_with_ci(path, ".cbr"))  return count_pages_cbr(path);
    if (str_ends_with_ci(path, ".epub")) return count_pages_epub(path);
    if (str_ends_with_ci(path, ".pdf"))  return count_pages_pdf(path);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Result record                                                       */
/* ------------------------------------------------------------------ */
typedef struct { char name[PATH_MAX]; int pages; } Result;
static int result_cmp(const void *a, const void *b)
{ return ((const Result *)a)->pages - ((const Result *)b)->pages; }

static Result g_results[4096];
static int    g_result_count = 0;

static const char *COMIC_EXTS[]  = { ".cbz", ".cbr", ".epub", ".pdf" };
#define COMIC_EXT_CNT 4

/* ------------------------------------------------------------------ */
/*  Directory analysis                                                  */
/* ------------------------------------------------------------------ */
static void analyze_directory(const char *dir_path)
{
    DIR *dir = opendir(dir_path);
    if (!dir) { LOG_E("Cannot open directory: %s", dir_path); return; }

    LOG_I("Analyzing directory: %s", dir_path);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) continue;

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            FileList *fl = filelist_create();
            find_files_by_extensions(full_path, COMIC_EXTS, COMIC_EXT_CNT,
                                     NULL, 0, 1, fl);
            if (fl->count > 0) {
                ProgressReporter pr;
                progress_init(&pr, fl->count, entry->d_name);
                int total = 0;
                for (int i = 0; i < fl->count; i++) {
                    if (safe_file_operation(fl->paths[i])) {
                        int p = count_pages_in_file(fl->paths[i]);
                        if (p > 0) total += p;
                    }
                    progress_update(&pr, 1);
                }
                progress_finish(&pr);
                if (g_result_count < 4096) {
                    snprintf(g_results[g_result_count].name,
                             sizeof(g_results[0].name), "%s", entry->d_name);
                    g_results[g_result_count].pages = total;
                    g_result_count++;
                }
                LOG_I("[DIR] %s: %d files, %d pages", entry->d_name, fl->count, total);
            }
            filelist_free(fl);
        } else if (S_ISREG(st.st_mode)) {
            int is_comic = 0;
            for (int i = 0; i < COMIC_EXT_CNT; i++)
                if (str_ends_with_ci(entry->d_name, COMIC_EXTS[i])) { is_comic=1; break; }
            if (!is_comic) continue;
            if (!safe_file_operation(full_path)) continue;
            int pages = count_pages_in_file(full_path);
            if (pages < 0) pages = 0;
            LOG_I("  %s: %d pages", entry->d_name, pages);
            if (g_result_count < 4096) {
                snprintf(g_results[g_result_count].name,
                         sizeof(g_results[0].name), "%s", entry->d_name);
                g_results[g_result_count].pages = pages;
                g_result_count++;
            }
        }
    }
    closedir(dir);
}

/* ------------------------------------------------------------------ */
/*  Display and save results                                            */
/* ------------------------------------------------------------------ */
static void display_results(void)
{
    if (g_result_count == 0) { LOG_I("No supported files found."); return; }
    qsort(g_results, g_result_count, sizeof(Result), result_cmp);

    LOG_I("\n==================================================");
    LOG_I("FINAL RESULTS (sorted by page count)");
    LOG_I("==================================================");
    int total = 0;
    for (int i = 0; i < g_result_count; i++) {
        LOG_I("%s: %d pages", g_results[i].name, g_results[i].pages);
        total += g_results[i].pages;
    }
    LOG_I("Total items: %d  |  Total pages: %d", g_result_count, total);

    int max_lines = g_result_count + 3;
    char **lines = malloc(max_lines * sizeof(char *));
    char  *bufs  = malloc(max_lines * 256);
    if (!lines || !bufs) { free(lines); free(bufs); return; }
    for (int i = 0; i < g_result_count; i++) {
        lines[i] = bufs + i * 256;
        snprintf(lines[i], 256, "%s: %d pages", g_results[i].name, g_results[i].pages);
    }
    lines[g_result_count]   = bufs + g_result_count * 256;
    lines[g_result_count+1] = bufs + (g_result_count+1) * 256;
    lines[g_result_count+2] = bufs + (g_result_count+2) * 256;
    strcpy(lines[g_result_count],   "");
    snprintf(lines[g_result_count+1], 256, "Total items: %d", g_result_count);
    snprintf(lines[g_result_count+2], 256, "Total pages: %d", total);
    save_results_to_file((const char **)lines, max_lines,
                         "page_count_results.txt", "COMIC/MANGA PAGE COUNT RESULTS");
    free(lines); free(bufs);
}

/* ------------------------------------------------------------------ */
/*  Entry point                                                         */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    log_init(LOG_INFO, NULL);
    char dir[PATH_MAX];
    if (argc >= 2) snprintf(dir, sizeof(dir), "%s", argv[1]);
    else           getcwd(dir, sizeof(dir));

    LOG_I("Starting comic/manga analysis in: %s", dir);
    analyze_directory(dir);
    display_results();
    log_close();
    return 0;
}
