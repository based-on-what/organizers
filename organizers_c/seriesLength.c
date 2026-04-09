/*
 * seriesLength.c
 * Translated from: seriesLength.py
 *
 * Purpose: TV Series Duration Analyzer.
 *   Treats each immediate subdirectory of the working directory as a
 *   TV series.  Recursively finds all video files inside it, sums
 *   their durations, and prints a table sorted by total duration.
 *   Results are saved to "series_durations.txt".
 *
 * Windows-native APIs used:
 *   - Windows Media Foundation (same as length.c) for video duration
 *   - FindFirstFile / FindNextFile for directory and file traversal
 *   - GetCurrentDirectoryA for default working directory
 *
 * Link libraries (MSVC):
 *   mfplat.lib  mfreadwrite.lib  mf.lib  mfuuid.lib  propsys.lib
 *
 * Compilation (MSVC):
 *   cl /W3 /std:c11 seriesLength.c shared_utils.c /Fe:seriesLength.exe ^
 *      mfplat.lib mfreadwrite.lib mf.lib mfuuid.lib propsys.lib
 * Compilation (MinGW):
 *   gcc -std=c11 -Wall seriesLength.c shared_utils.c -o seriesLength.exe ^
 *       -lmfplat -lmfreadwrite -lmf -lmfuuid -lpropsys -lole32
 */

#include "shared_utils.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propvarutil.h>

#ifndef MF_PD_DURATION
DEFINE_GUID(MF_PD_DURATION,
    0x6c990d33, 0xbb8e, 0x477a, 0x85, 0x98, 0x0d, 0x5d, 0x96, 0xfc, 0xd8, 0x8a);
#endif

/* ------------------------------------------------------------------ */
/*  Default configuration (mirrors Python script defaults)             */
/* ------------------------------------------------------------------ */
static const char *VIDEO_EXTS[] = {
    ".mp4", ".avi", ".mkv", ".mov", ".wmv", ".flv", ".webm"
};
#define VIDEO_EXT_COUNT 7

static const char *EXCLUDED_DIRS[] = {
    "Sub", "Subs", "Subtitles", "Featurettes", "Extras"
};
#define EXCLUDED_DIR_COUNT 5

/* ------------------------------------------------------------------ */
/*  Get video duration via Media Foundation                            */
/* ------------------------------------------------------------------ */
static double get_video_duration_mf(const char *path)
{
    wchar_t wpath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, MAX_PATH);

    IMFSourceReader *pReader = NULL;
    HRESULT hr = MFCreateSourceReaderFromURL(wpath, NULL, &pReader);
    if (FAILED(hr)) return 0.0;

    PROPVARIANT pv;
    PropVariantInit(&pv);
    hr = pReader->lpVtbl->GetPresentationAttribute(
             pReader,
             (DWORD)MF_SOURCE_READER_MEDIASOURCE,
             &MF_PD_DURATION, &pv);

    double duration = 0.0;
    if (SUCCEEDED(hr) && pv.vt == VT_UI8)
        duration = (double)pv.uhVal.QuadPart / 10000000.0;

    PropVariantClear(&pv);
    pReader->lpVtbl->Release(pReader);
    return duration;
}

/* ------------------------------------------------------------------ */
/*  Series result record                                                */
/* ------------------------------------------------------------------ */
typedef struct {
    char   name[MAX_PATH];
    double total_seconds;
    int    file_count;
} SeriesResult;

static SeriesResult g_results[2048];
static int          g_result_count = 0;

/* ------------------------------------------------------------------ */
/*  Process one series directory                                        */
/* ------------------------------------------------------------------ */
static void process_series(const char *series_dir, const char *series_name)
{
    FileList *fl = filelist_create();
    find_files_by_extensions(series_dir,
                              VIDEO_EXTS,    VIDEO_EXT_COUNT,
                              EXCLUDED_DIRS, EXCLUDED_DIR_COUNT,
                              1, fl);

    if (fl->count == 0) {
        LOG_I("[DIR] %s: No video files found", series_name);
        filelist_free(fl);
        /* Still record the series with 0 duration */
        if (g_result_count < 2048) {
            _snprintf(g_results[g_result_count].name,
                      sizeof(g_results[0].name), "%s", series_name);
            g_results[g_result_count].total_seconds = 0.0;
            g_results[g_result_count].file_count    = 0;
            g_result_count++;
        }
        return;
    }

    ProgressReporter pr;
    progress_init(&pr, fl->count, series_name);

    double total = 0.0;
    for (int i = 0; i < fl->count; i++) {
        double d = get_video_duration_mf(fl->paths[i]);
        if (d < 0) d = 0;
        total += d;
        progress_update(&pr, 1);
    }
    progress_finish(&pr);

    char dur_str[32];
    format_duration(dur_str, sizeof(dur_str), total);
    LOG_I("[TV]  %s: %d files, %s", series_name, fl->count, dur_str);

    if (g_result_count < 2048) {
        _snprintf(g_results[g_result_count].name,
                  sizeof(g_results[0].name), "%s", series_name);
        g_results[g_result_count].total_seconds = total;
        g_results[g_result_count].file_count    = fl->count;
        g_result_count++;
    }

    filelist_free(fl);
}

/* ------------------------------------------------------------------ */
/*  Result comparison (sort by duration ascending)                     */
/* ------------------------------------------------------------------ */
static int series_cmp(const void *a, const void *b)
{
    double da = ((const SeriesResult *)a)->total_seconds;
    double db = ((const SeriesResult *)b)->total_seconds;
    if (da < db) return -1;
    if (da > db) return  1;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Save results to file                                                */
/* ------------------------------------------------------------------ */
static void save_results(const char *output_file)
{
    int max_lines = g_result_count + 5;
    char **lines = (char **)malloc(max_lines * sizeof(char *));
    char  *bufs  = (char  *)malloc(max_lines * 256);
    if (!lines || !bufs) { free(lines); free(bufs); return; }

    int lc = 0;
    for (int i = 0; i < g_result_count; i++) {
        char dur_str[32];
        format_duration(dur_str, sizeof(dur_str), g_results[i].total_seconds);
        lines[lc] = bufs + lc * 256;
        _snprintf(lines[lc], 256, "%s: %s", g_results[i].name, dur_str);
        lc++;
    }

    save_results_to_file((const char **)lines, lc,
                          output_file, "TV SERIES DURATION ANALYSIS");
    free(lines);
    free(bufs);
}

/* ------------------------------------------------------------------ */
/*  Entry point                                                         */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    log_init(LOG_INFO, NULL);

    char base_dir[MAX_PATH * 3];
    if (argc >= 2) {
        wchar_t warg[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, argv[1], -1, warg, MAX_PATH);
        wide_to_utf8(warg, base_dir, sizeof(base_dir));
    } else {
        wchar_t wdir_buf[MAX_PATH];
        GetCurrentDirectoryW(MAX_PATH, wdir_buf);
        wide_to_utf8(wdir_buf, base_dir, sizeof(base_dir));
    }

    LOG_I("TV Series Duration Analyzer");
    LOG_I("Base directory: %s", base_dir);

    /* Validate */
    wchar_t wbase[MAX_PATH];
    utf8_to_wide(base_dir, wbase, MAX_PATH);
    DWORD attrs = GetFileAttributesW(wbase);
    if (attrs == INVALID_FILE_ATTRIBUTES ||
        !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        LOG_E("Directory does not exist: %s", base_dir);
        return 1;
    }

    /* Initialize Windows Media Foundation */
    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    if (FAILED(hr)) {
        LOG_E("MFStartup failed (0x%08X). Media Foundation unavailable.",
              (unsigned)hr);
        return 1;
    }

    /* Enumerate immediate subdirectories as series */
    wchar_t wpattern[MAX_PATH];
    _snwprintf(wpattern, MAX_PATH, L"%s\\*", wbase);

    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(wpattern, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        LOG_E("Cannot enumerate: %s", base_dir);
        MFShutdown();
        return 1;
    }

    LOG_I("Processing video files in series directories...");

    do {
        if (wcscmp(ffd.cFileName, L".") == 0 ||
            wcscmp(ffd.cFileName, L"..") == 0)
            continue;
        if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            continue;

        char fname_u8[MAX_PATH * 3];
        wide_to_utf8(ffd.cFileName, fname_u8, sizeof(fname_u8));

        /* Skip excluded directories */
        int excluded = 0;
        for (int i = 0; i < EXCLUDED_DIR_COUNT; i++)
            if (_stricmp(fname_u8, EXCLUDED_DIRS[i]) == 0) {
                excluded = 1; break;
            }
        if (excluded) continue;

        char series_path[MAX_PATH * 3];
        _snprintf(series_path, sizeof(series_path), "%s\\%s",
                  base_dir, fname_u8);
        process_series(series_path, fname_u8);

    } while (FindNextFileW(hFind, &ffd));
    FindClose(hFind);

    MFShutdown();

    if (g_result_count == 0) {
        LOG_W("No series directories with video files found!");
        return 0;
    }

    /* Sort by duration */
    qsort(g_results, g_result_count, sizeof(SeriesResult), series_cmp);

    /* Print sorted table */
    LOG_I("\nSeries sorted by total duration:");
    double grand_total = 0;
    for (int i = 0; i < g_result_count; i++) {
        char dur_str[32];
        format_duration(dur_str, sizeof(dur_str), g_results[i].total_seconds);
        LOG_I("%s: %s", g_results[i].name, dur_str);
        grand_total += g_results[i].total_seconds;
    }

    char gt_str[32];
    format_duration(gt_str, sizeof(gt_str), grand_total);
    LOG_I("\nOverall Statistics:");
    LOG_I("Total series:   %d", g_result_count);
    LOG_I("Total duration: %s", gt_str);

    /* Save */
    const char *output_file = "series_durations.txt";
    save_results(output_file);
    LOG_I("Results saved to %s", output_file);

    log_close();
    return 0;
}
