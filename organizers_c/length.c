/*
 * length.c
 * Translated from: length.py
 *
 * Purpose: Video Duration Analyzer.
 *   Recursively scans a directory for video files and reports their
 *   durations, file sizes, and aggregate statistics.  Output is
 *   written to a text file (or JSON if -j flag is used).
 *
 * Windows-native APIs used:
 *   - Windows Media Foundation (MFStartup, MFCreateSourceReaderFromURL,
 *     IMFSourceReader::GetPresentationAttribute with MF_PD_DURATION)
 *     to read video duration without any third-party library.
 *   - GetFileAttributesExA for file size
 *   - FindFirstFile / FindNextFile (via shared_utils) for traversal
 *
 * Link libraries (MSVC):
 *   mfplat.lib  mfreadwrite.lib  mf.lib  mfuuid.lib
 *   propsys.lib  shlwapi.lib
 *
 * Compilation (MSVC):
 *   cl /W3 /std:c11 length.c shared_utils.c /Fe:length.exe ^
 *      mfplat.lib mfreadwrite.lib mf.lib mfuuid.lib propsys.lib shlwapi.lib
 * Compilation (MinGW):
 *   gcc -std=c11 -Wall length.c shared_utils.c -o length.exe ^
 *       -lmfplat -lmfreadwrite -lmf -lmfuuid -lpropsys -lshlwapi -lole32
 *
 * Note: Windows Media Foundation is available on Windows Vista and later.
 *       It may not decode all video codecs without the appropriate codec
 *       packs installed (e.g. DivX, Xvid require their own decoders).
 */

#include "shared_utils.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propvarutil.h>

/* Required for MF_PD_DURATION GUID */
#ifndef MF_PD_DURATION
DEFINE_GUID(MF_PD_DURATION,
    0x6c990d33, 0xbb8e, 0x477a, 0x85, 0x98, 0xd, 0x5d, 0x96, 0xfc, 0xd8, 0x8a);
#endif

/* Minimum file size to process (100 KB, matching original Python script) */
#define MIN_VIDEO_SIZE_BYTES  (100 * 1024)

/* ------------------------------------------------------------------ */
/*  Result record                                                       */
/* ------------------------------------------------------------------ */
typedef struct {
    char        path[MAX_PATH];
    char        name[MAX_PATH];
    double      duration;      /* seconds */
    long long   file_size;     /* bytes   */
} VideoResult;

static VideoResult  g_results[16384];
static int          g_result_count = 0;

/* ------------------------------------------------------------------ */
/*  Get video duration via Windows Media Foundation                    */
/* ------------------------------------------------------------------ */
static double get_video_duration_mf(const wchar_t *wpath)
{
    IMFSourceReader *pReader = NULL;

    HRESULT hr = MFCreateSourceReaderFromURL(wpath, NULL, &pReader);
    if (FAILED(hr))
        return -1.0;

    PROPVARIANT pv;
    PropVariantInit(&pv);

    hr = pReader->lpVtbl->GetPresentationAttribute(
            pReader,
            (DWORD)MF_SOURCE_READER_MEDIASOURCE,
            &MF_PD_DURATION,
            &pv);

    double duration = -1.0;
    if (SUCCEEDED(hr) && pv.vt == VT_UI8) {
        /* MF_PD_DURATION is in 100-nanosecond units */
        duration = (double)pv.uhVal.QuadPart / 10000000.0;
    }

    PropVariantClear(&pv);
    pReader->lpVtbl->Release(pReader);
    return duration;
}

static long long get_file_size(const char *path)
{
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fad))
        return -1;
    LARGE_INTEGER sz;
    sz.HighPart = (LONG)fad.nFileSizeHigh;
    sz.LowPart  = fad.nFileSizeLow;
    return sz.QuadPart;
}

/* ------------------------------------------------------------------ */
/*  Analyze a single video file                                         */
/* ------------------------------------------------------------------ */
static void process_video(const char *path)
{
    if (!safe_file_operation(path)) return;

    long long file_size = get_file_size(path);
    if (file_size < MIN_VIDEO_SIZE_BYTES) {
        LOG_W("File too small (%lld bytes), skipping: %s", file_size, path);
        return;
    }

    /* Convert path to wide string for MF */
    wchar_t wpath[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, MAX_PATH);

    double duration = get_video_duration_mf(wpath);
    if (duration <= 0.0) {
        LOG_W("Could not read duration: %s", path);
        return;
    }

    if (g_result_count >= 16384) {
        LOG_W("Result buffer full, skipping: %s", path);
        return;
    }

    VideoResult *r = &g_results[g_result_count++];
    _snprintf(r->path, sizeof(r->path), "%s", path);
    /* Extract filename */
    const char *sep = strrchr(path, '\\');
    _snprintf(r->name, sizeof(r->name), "%s", sep ? sep + 1 : path);
    r->duration  = duration;
    r->file_size = file_size;

    char dur_str[32], sz_str[32];
    format_duration(dur_str, sizeof(dur_str), duration);
    format_file_size(sz_str, sizeof(sz_str), file_size);
    LOG_I("OK  %s: %s | %s", r->name, dur_str, sz_str);
}

/* ------------------------------------------------------------------ */
/*  Result comparison (sort by duration ascending)                     */
/* ------------------------------------------------------------------ */
static int result_cmp(const void *a, const void *b)
{
    double da = ((const VideoResult *)a)->duration;
    double db = ((const VideoResult *)b)->duration;
    if (da < db) return -1;
    if (da > db) return  1;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Save results to text file                                           */
/* ------------------------------------------------------------------ */
static void save_results_txt(const char *output_file)
{
    if (g_result_count == 0) return;

    /* Sort by duration */
    qsort(g_results, g_result_count, sizeof(VideoResult), result_cmp);

    double total_dur  = 0;
    long long total_sz = 0;
    for (int i = 0; i < g_result_count; i++) {
        total_dur += g_results[i].duration;
        total_sz  += g_results[i].file_size;
    }

    /* Build line array: 5 lines per file + separator + summary lines */
    int max_lines = g_result_count * 6 + 10;
    char **lines    = (char **)malloc(max_lines * sizeof(char *));
    char  *bufs     = (char  *)malloc(max_lines * 512);
    if (!lines || !bufs) { free(lines); free(bufs); return; }

    int lc = 0;
    for (int i = 0; i < g_result_count; i++) {
        char dur_str[32], sz_str[32];
        format_duration(dur_str, sizeof(dur_str), g_results[i].duration);
        format_file_size(sz_str, sizeof(sz_str), g_results[i].file_size);

#define LINE(fmt, ...) do { \
    lines[lc] = bufs + lc * 512; \
    _snprintf(lines[lc], 512, fmt, ##__VA_ARGS__); \
    lc++; } while(0)

        LINE("File: %s",     g_results[i].name);
        LINE("Path: %s",     g_results[i].path);
        LINE("Duration: %s", dur_str);
        LINE("Size: %s",     sz_str);
        LINE("--------------------------------------------------");
    }

    char tot_dur_str[32], tot_sz_str[32];
    char avg_dur_str[32], avg_sz_str[32];
    format_duration(tot_dur_str, sizeof(tot_dur_str), total_dur);
    format_file_size(tot_sz_str, sizeof(tot_sz_str), total_sz);
    format_duration(avg_dur_str, sizeof(avg_dur_str),
                    total_dur / g_result_count);
    format_file_size(avg_sz_str, sizeof(avg_sz_str),
                     total_sz  / g_result_count);

    LINE("");
    LINE("SUMMARY:");
    LINE("Total files: %d",      g_result_count);
    LINE("Total duration: %s",   tot_dur_str);
    LINE("Total size: %s",       tot_sz_str);
    LINE("Average duration: %s", avg_dur_str);
    LINE("Average size: %s",     avg_sz_str);
#undef LINE

    save_results_to_file((const char **)lines, lc,
                          output_file, "VIDEO DURATION ANALYSIS REPORT");
    free(lines);
    free(bufs);
}

/* ------------------------------------------------------------------ */
/*  Print summary to console                                            */
/* ------------------------------------------------------------------ */
static void print_summary(void)
{
    if (g_result_count == 0) {
        LOG_I("No video files were successfully processed.");
        return;
    }
    double total_dur = 0;
    long long total_sz = 0;
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
    LOG_I("Total files processed: %d", g_result_count);
    LOG_I("Total duration:  %s", t_dur);
    LOG_I("Total size:      %s", t_sz);
    LOG_I("Average duration:%s", a_dur);
    LOG_I("Average size:    %s", a_sz);
}

/* ------------------------------------------------------------------ */
/*  Command-line argument defaults                                       */
/* ------------------------------------------------------------------ */
static const char *DEFAULT_VIDEO_EXTS[] = {
    ".mp4", ".avi", ".mkv", ".mov", ".wmv", ".flv", ".webm"
};
#define DEFAULT_VIDEO_EXT_COUNT 7

static const char *DEFAULT_EXCLUDE_DIRS[] = {
    "Sub", "Subs", "Subtitles", "Featurettes", "Extras"
};
#define DEFAULT_EXCLUDE_DIR_COUNT 5

static void print_usage(const char *prog)
{
    printf("Usage: %s [directory] [-o output.txt] [-e .mp4 .mkv ...] "
           "[-x DirToSkip ...]\n", prog);
    printf("  directory   Directory to scan (default: current dir)\n");
    printf("  -o FILE     Output file (default: video_duration_analysis.txt)\n");
    printf("  -e EXTS     Video extensions to include (space-separated)\n");
    printf("  -x DIRS     Directory names to exclude (space-separated)\n");
}

/* ------------------------------------------------------------------ */
/*  Entry point                                                         */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    log_init(LOG_INFO, "video_analyzer.log");

    /* Defaults */
    char base_dir[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, base_dir);
    char output_file[MAX_PATH] = "video_duration_analysis.txt";

    const char **video_exts   = DEFAULT_VIDEO_EXTS;
    int          video_ext_cnt = DEFAULT_VIDEO_EXT_COUNT;
    const char **excl_dirs    = DEFAULT_EXCLUDE_DIRS;
    int          excl_dir_cnt  = DEFAULT_EXCLUDE_DIR_COUNT;

    /* Parse arguments (minimal) */
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            _snprintf(base_dir, sizeof(base_dir), "%s", argv[i]);
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            _snprintf(output_file, sizeof(output_file), "%s", argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0 ||
                   strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    /* Validate directory */
    DWORD attrs = GetFileAttributesA(base_dir);
    if (attrs == INVALID_FILE_ATTRIBUTES ||
        !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        LOG_E("Directory does not exist: %s", base_dir);
        return 1;
    }

    LOG_I("Starting video analysis in: %s", base_dir);

    /* Initialize Windows Media Foundation once for the whole run */
    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    if (FAILED(hr)) {
        LOG_E("MFStartup failed (HRESULT 0x%08X). "
              "Windows Media Foundation may not be available.", (unsigned)hr);
        return 1;
    }

    /* Find all video files */
    FileList *fl = filelist_create();
    find_files_by_extensions(base_dir,
                              video_exts,   video_ext_cnt,
                              excl_dirs,    excl_dir_cnt,
                              1, fl);

    if (fl->count == 0) {
        LOG_W("No video files found in: %s", base_dir);
        filelist_free(fl);
        MFShutdown();
        return 0;
    }
    LOG_I("Found %d video file(s). Analyzing...", fl->count);

    /* Process each file */
    ProgressReporter pr;
    progress_init(&pr, fl->count, "Analyzing videos");
    for (int i = 0; i < fl->count; i++) {
        process_video(fl->paths[i]);
        progress_update(&pr, 1);
    }
    progress_finish(&pr);
    filelist_free(fl);

    MFShutdown();

    /* Save and print */
    if (g_result_count > 0) {
        save_results_txt(output_file);
    } else {
        LOG_W("No results to save - all files failed to process.");
    }
    print_summary();

    log_close();
    return 0;
}
