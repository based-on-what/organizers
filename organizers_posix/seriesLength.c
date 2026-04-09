/*
 * seriesLength.c  (POSIX version)
 * Translated from: seriesLength.py
 *
 * Purpose: TV Series Duration Analyzer.
 *   Each immediate subdirectory is treated as one series.
 *
 * POSIX differences vs Windows version:
 *   - Video duration: libavformat instead of Media Foundation.
 *   - Directory traversal: opendir/readdir via shared_utils.
 *
 * Compilation (Linux):
 *   gcc -std=c11 -Wall -O2 seriesLength.c shared_utils.c -o seriesLength \
 *       $(pkg-config --cflags --libs libavformat libavutil)
 * Compilation (macOS):
 *   clang -std=c11 -Wall -O2 seriesLength.c shared_utils.c -o seriesLength \
 *       $(pkg-config --cflags --libs libavformat libavutil)
 */

#include "shared_utils.h"
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>

static const char *VIDEO_EXTS[]     = { ".mp4",".avi",".mkv",".mov",".wmv",".flv",".webm" };
static const char *EXCLUDED_DIRS[]  = { "Sub","Subs","Subtitles","Featurettes","Extras" };
#define VIDEO_EXT_COUNT   7
#define EXCLUDED_DIR_COUNT 5

static double get_video_duration(const char *path)
{
    AVFormatContext *ctx = NULL;
    if (avformat_open_input(&ctx, path, NULL, NULL) != 0) return 0.0;
    if (avformat_find_stream_info(ctx, NULL) < 0) {
        avformat_close_input(&ctx); return 0.0;
    }
    double d = (ctx->duration != AV_NOPTS_VALUE)
               ? (double)ctx->duration / (double)AV_TIME_BASE
               : 0.0;
    avformat_close_input(&ctx);
    return d;
}

typedef struct { char name[PATH_MAX]; double total_seconds; int file_count; } SeriesResult;
static SeriesResult g_results[2048]; static int g_result_count = 0;

static int series_cmp(const void *a, const void *b)
{
    double da = ((const SeriesResult *)a)->total_seconds;
    double db = ((const SeriesResult *)b)->total_seconds;
    return (da > db) - (da < db);
}

static void process_series(const char *series_dir, const char *series_name)
{
    FileList *fl = filelist_create();
    find_files_by_extensions(series_dir, VIDEO_EXTS, VIDEO_EXT_COUNT,
                              EXCLUDED_DIRS, EXCLUDED_DIR_COUNT, 1, fl);
    if (fl->count == 0) {
        LOG_I("[DIR] %s: No video files found", series_name);
        filelist_free(fl);
        if (g_result_count < 2048) {
            snprintf(g_results[g_result_count].name, PATH_MAX, "%s", series_name);
            g_results[g_result_count].total_seconds = 0.0;
            g_results[g_result_count++].file_count  = 0;
        }
        return;
    }

    ProgressReporter pr; progress_init(&pr, fl->count, series_name);
    double total = 0.0;
    for (int i = 0; i < fl->count; i++) {
        total += get_video_duration(fl->paths[i]);
        progress_update(&pr, 1);
    }
    progress_finish(&pr);

    char dur_str[32]; format_duration(dur_str, sizeof(dur_str), total);
    LOG_I("[TV]  %s: %d files, %s", series_name, fl->count, dur_str);

    if (g_result_count < 2048) {
        snprintf(g_results[g_result_count].name, PATH_MAX, "%s", series_name);
        g_results[g_result_count].total_seconds = total;
        g_results[g_result_count++].file_count  = fl->count;
    }
    filelist_free(fl);
}

int main(int argc, char *argv[])
{
    log_init(LOG_INFO, NULL);
    char base_dir[PATH_MAX];
    if (argc >= 2) snprintf(base_dir, sizeof(base_dir), "%s", argv[1]);
    else           getcwd(base_dir, sizeof(base_dir));

    struct stat bst;
    if (stat(base_dir, &bst) != 0 || !S_ISDIR(bst.st_mode)) {
        LOG_E("Directory does not exist: %s", base_dir); return 1;
    }
    LOG_I("TV Series Duration Analyzer");
    LOG_I("Base directory: %s", base_dir);
    LOG_I("Processing series directories...");

    DIR *dir = opendir(base_dir);
    if (!dir) { LOG_E("Cannot open: %s", base_dir); return 1; }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char series_path[PATH_MAX];
        snprintf(series_path, sizeof(series_path), "%s/%s", base_dir, entry->d_name);
        struct stat st;
        if (stat(series_path, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        int excluded = 0;
        for (int i = 0; i < EXCLUDED_DIR_COUNT; i++)
            if (strcasecmp(entry->d_name, EXCLUDED_DIRS[i]) == 0) { excluded = 1; break; }
        if (!excluded) process_series(series_path, entry->d_name);
    }
    closedir(dir);

    if (g_result_count == 0) { LOG_W("No series found!"); return 0; }

    qsort(g_results, g_result_count, sizeof(SeriesResult), series_cmp);

    double grand_total = 0;
    LOG_I("\nSeries sorted by total duration:");
    for (int i = 0; i < g_result_count; i++) {
        char dur_str[32]; format_duration(dur_str, sizeof(dur_str), g_results[i].total_seconds);
        LOG_I("%s: %s", g_results[i].name, dur_str);
        grand_total += g_results[i].total_seconds;
    }
    char gt_str[32]; format_duration(gt_str, sizeof(gt_str), grand_total);
    LOG_I("\nOverall: %d series, total %s", g_result_count, gt_str);

    /* Save */
    int    max_lines = g_result_count + 2;
    char **lines = malloc(max_lines * sizeof(char *));
    char  *bufs  = malloc(max_lines * 256);
    if (lines && bufs) {
        int lc = 0;
        for (int i = 0; i < g_result_count; i++) {
            char dur_str[32]; format_duration(dur_str, sizeof(dur_str), g_results[i].total_seconds);
            lines[lc] = bufs + lc * 256;
            snprintf(lines[lc], 256, "%s: %s", g_results[i].name, dur_str);
            lc++;
        }
        save_results_to_file((const char **)lines, lc,
                              "series_durations.txt", "TV SERIES DURATION ANALYSIS");
        LOG_I("Results saved to series_durations.txt");
    }
    free(lines); free(bufs);
    log_close();
    return 0;
}
