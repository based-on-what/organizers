/*
 * length.c  (POSIX version)
 * Translated from: length.py
 *
 * Purpose: Video Duration Analyzer.
 *
 * POSIX differences vs Windows version:
 *   - Video duration: libavformat (FFmpeg) instead of Media Foundation.
 *     avformat_open_input + avformat_find_stream_info + ctx->duration.
 *   - File size: stat() instead of GetFileAttributesExA.
 *   - Directory traversal: opendir/readdir via shared_utils.
 *
 * Dependencies:
 *   libavformat, libavutil  (part of FFmpeg)
 *
 * Install FFmpeg dev libraries:
 *   Linux (Debian/Ubuntu):  sudo apt install libavformat-dev libavutil-dev
 *   Linux (Fedora):         sudo dnf install ffmpeg-devel
 *   macOS (Homebrew):       brew install ffmpeg
 *
 * Compilation (Linux):
 *   gcc -std=c11 -Wall -O2 length.c shared_utils.c -o length \
 *       $(pkg-config --cflags --libs libavformat libavutil)
 * Compilation (macOS):
 *   clang -std=c11 -Wall -O2 length.c shared_utils.c -o length \
 *       $(pkg-config --cflags --libs libavformat libavutil)
 */

#include "shared_utils.h"
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>

#define MIN_VIDEO_SIZE_BYTES (100 * 1024)

/* ------------------------------------------------------------------ */
/*  Get video duration via libavformat                                  */
/* ------------------------------------------------------------------ */
static double get_video_duration(const char *path)
{
    AVFormatContext *ctx = NULL;
    if (avformat_open_input(&ctx, path, NULL, NULL) != 0)
        return -1.0;
    if (avformat_find_stream_info(ctx, NULL) < 0) {
        avformat_close_input(&ctx);
        return -1.0;
    }
    double duration = -1.0;
    if (ctx->duration != AV_NOPTS_VALUE)
        duration = (double)ctx->duration / (double)AV_TIME_BASE;
    avformat_close_input(&ctx);
    return duration;
}

static long long get_file_size(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (long long)st.st_size;
}

/* ------------------------------------------------------------------ */
/*  Result record                                                       */
/* ------------------------------------------------------------------ */
typedef struct {
    char      path[PATH_MAX];
    char      name[PATH_MAX];
    double    duration;
    long long file_size;
} VideoResult;

static VideoResult g_results[16384];
static int         g_result_count = 0;

static int result_cmp(const void *a, const void *b)
{
    double da = ((const VideoResult *)a)->duration;
    double db = ((const VideoResult *)b)->duration;
    return (da > db) - (da < db);
}

/* ------------------------------------------------------------------ */
/*  Process a single video file                                         */
/* ------------------------------------------------------------------ */
static void process_video(const char *path)
{
    if (!safe_file_operation(path)) return;

    long long file_size = get_file_size(path);
    if (file_size < MIN_VIDEO_SIZE_BYTES) {
        LOG_W("File too small (%lld bytes), skipping: %s", file_size, path);
        return;
    }

    double duration = get_video_duration(path);
    if (duration <= 0.0) {
        LOG_W("Could not read duration: %s", path);
        return;
    }

    if (g_result_count >= 16384) return;

    VideoResult *r = &g_results[g_result_count++];
    snprintf(r->path, sizeof(r->path), "%s", path);
    const char *sep = strrchr(path, '/');
    snprintf(r->name, sizeof(r->name), "%s", sep ? sep + 1 : path);
    r->duration  = duration;
    r->file_size = file_size;

    char dur_str[32], sz_str[32];
    format_duration(dur_str, sizeof(dur_str), duration);
    format_file_size(sz_str, sizeof(sz_str), file_size);
    LOG_I("OK  %s: %s | %s", r->name, dur_str, sz_str);
}

/* ------------------------------------------------------------------ */
/*  Save results                                                        */
/* ------------------------------------------------------------------ */
static void save_results_txt(const char *output_file)
{
    if (g_result_count == 0) return;
    qsort(g_results, g_result_count, sizeof(VideoResult), result_cmp);

    double    total_dur = 0;
    long long total_sz  = 0;
    for (int i = 0; i < g_result_count; i++) {
        total_dur += g_results[i].duration;
        total_sz  += g_results[i].file_size;
    }

    int    max_lines = g_result_count * 6 + 10;
    char **lines     = malloc(max_lines * sizeof(char *));
    char  *bufs      = malloc(max_lines * 512);
    if (!lines || !bufs) { free(lines); free(bufs); return; }

    int lc = 0;
#define LINE(fmt, ...) do { \
    lines[lc] = bufs + lc * 512; \
    snprintf(lines[lc], 512, fmt, ##__VA_ARGS__); lc++; } while(0)

    for (int i = 0; i < g_result_count; i++) {
        char dur_str[32], sz_str[32];
        format_duration(dur_str, sizeof(dur_str), g_results[i].duration);
        format_file_size(sz_str, sizeof(sz_str), g_results[i].file_size);
        LINE("File: %s",     g_results[i].name);
        LINE("Path: %s",     g_results[i].path);
        LINE("Duration: %s", dur_str);
        LINE("Size: %s",     sz_str);
        LINE("--------------------------------------------------");
    }

    char t_dur[32], t_sz[32], a_dur[32], a_sz[32];
    format_duration (t_dur, sizeof(t_dur), total_dur);
    format_file_size(t_sz,  sizeof(t_sz),  total_sz);
    format_duration (a_dur, sizeof(a_dur), total_dur / g_result_count);
    format_file_size(a_sz,  sizeof(a_sz),  total_sz  / g_result_count);
    LINE(""); LINE("SUMMARY:");
    LINE("Total files: %d",       g_result_count);
    LINE("Total duration: %s",    t_dur);
    LINE("Total size: %s",        t_sz);
    LINE("Average duration: %s",  a_dur);
    LINE("Average size: %s",      a_sz);
#undef LINE

    save_results_to_file((const char **)lines, lc,
                          output_file, "VIDEO DURATION ANALYSIS REPORT");
    free(lines); free(bufs);
}

static void print_summary(void)
{
    if (g_result_count == 0) {
        LOG_I("No video files were successfully processed."); return;
    }
    double total_dur = 0; long long total_sz = 0;
    for (int i = 0; i < g_result_count; i++) {
        total_dur += g_results[i].duration;
        total_sz  += g_results[i].file_size;
    }
    char t_dur[32], t_sz[32], a_dur[32], a_sz[32];
    format_duration (t_dur, sizeof(t_dur), total_dur);
    format_file_size(t_sz,  sizeof(t_sz),  total_sz);
    format_duration (a_dur, sizeof(a_dur), total_dur / g_result_count);
    format_file_size(a_sz,  sizeof(a_sz),  total_sz  / g_result_count);
    LOG_I("==================================================");
    LOG_I("SUMMARY STATISTICS");
    LOG_I("==================================================");
    LOG_I("Total files:      %d",  g_result_count);
    LOG_I("Total duration:   %s",  t_dur);
    LOG_I("Total size:       %s",  t_sz);
    LOG_I("Average duration: %s",  a_dur);
    LOG_I("Average size:     %s",  a_sz);
}

/* ------------------------------------------------------------------ */
/*  Defaults                                                            */
/* ------------------------------------------------------------------ */
static const char *DEFAULT_VIDEO_EXTS[]    = { ".mp4",".avi",".mkv",".mov",".wmv",".flv",".webm" };
static const char *DEFAULT_EXCLUDE_DIRS[]  = { "Sub","Subs","Subtitles","Featurettes","Extras" };
#define DEFAULT_VIDEO_EXT_COUNT   7
#define DEFAULT_EXCLUDE_DIR_COUNT 5

/* ------------------------------------------------------------------ */
/*  Entry point                                                         */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    log_init(LOG_INFO, "video_analyzer.log");

    char base_dir[PATH_MAX];   getcwd(base_dir, sizeof(base_dir));
    char output_file[PATH_MAX]; snprintf(output_file, sizeof(output_file),
                                         "video_duration_analysis.txt");

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-')
            snprintf(base_dir, sizeof(base_dir), "%s", argv[i]);
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            snprintf(output_file, sizeof(output_file), "%s", argv[++i]);
    }

    struct stat st;
    if (stat(base_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        LOG_E("Directory does not exist: %s", base_dir); return 1;
    }
    LOG_I("Starting video analysis in: %s", base_dir);

    FileList *fl = filelist_create();
    find_files_by_extensions(base_dir,
                              DEFAULT_VIDEO_EXTS,   DEFAULT_VIDEO_EXT_COUNT,
                              DEFAULT_EXCLUDE_DIRS, DEFAULT_EXCLUDE_DIR_COUNT,
                              1, fl);
    if (fl->count == 0) {
        LOG_W("No video files found."); filelist_free(fl); return 0;
    }
    LOG_I("Found %d video file(s). Analyzing...", fl->count);

    ProgressReporter pr;
    progress_init(&pr, fl->count, "Analyzing videos");
    for (int i = 0; i < fl->count; i++) {
        process_video(fl->paths[i]);
        progress_update(&pr, 1);
    }
    progress_finish(&pr);
    filelist_free(fl);

    if (g_result_count > 0) save_results_txt(output_file);
    else LOG_W("No results to save.");
    print_summary();

    log_close();
    return 0;
}
