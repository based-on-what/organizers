/*
 * shared_utils.h
 * Translated from: shared_utils.py
 *
 * Purpose: Common utilities used by all organizer tools.
 *   - Timestamped logging (Win32 GetLocalTime)
 *   - File size and duration formatting
 *   - File validation (GetFileAttributesEx / CreateFile)
 *   - Recursive file finder (FindFirstFile / FindNextFile)
 *   - ZIP central-directory reader (for CBZ, EPUB, DOCX internals)
 *   - Result file writer
 *   - Progress reporter
 *
 * Compilation (MSVC):  cl /W3 shared_utils.c ...
 * Compilation (MinGW): gcc -std=c11 shared_utils.c ...
 */

#ifndef SHARED_UTILS_H
#define SHARED_UTILS_H

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------------------ */
/*  Logging                                                             */
/* ------------------------------------------------------------------ */
#define LOG_DEBUG   0
#define LOG_INFO    1
#define LOG_WARNING 2
#define LOG_ERROR   3

extern int   g_log_level;
extern FILE *g_log_file;

void log_init(int level, const char *log_file_path);
void log_msg (int level, const char *fmt, ...);
void log_close(void);

/* Convenience macros */
#define LOG_D(...)  log_msg(LOG_DEBUG,   __VA_ARGS__)
#define LOG_I(...)  log_msg(LOG_INFO,    __VA_ARGS__)
#define LOG_W(...)  log_msg(LOG_WARNING, __VA_ARGS__)
#define LOG_E(...)  log_msg(LOG_ERROR,   __VA_ARGS__)

/* ------------------------------------------------------------------ */
/*  Formatting helpers                                                  */
/* ------------------------------------------------------------------ */
void format_file_size(char *buf, size_t buf_size, long long size_bytes);
void format_duration (char *buf, size_t buf_size, double seconds);

/* ------------------------------------------------------------------ */
/*  File validation (Win32)                                             */
/* ------------------------------------------------------------------ */
/* Returns 1 if path is a file with size >= min_size, 0 otherwise */
int validate_file_path(const char *path, long long min_size);

/* Returns 1 if file exists, is non-empty, and is readable, 0 otherwise */
int safe_file_operation(const char *path);

/* ------------------------------------------------------------------ */
/*  Result writer                                                        */
/* ------------------------------------------------------------------ */
/*
 * Writes an array of text lines to output_file, preceded by a
 * title header.  Returns 1 on success, 0 on failure.
 */
int save_results_to_file(const char **lines, int line_count,
                          const char *output_file, const char *title);

/* ------------------------------------------------------------------ */
/*  Dynamic file list                                                    */
/* ------------------------------------------------------------------ */
typedef struct {
    char  **paths;
    int     count;
    int     capacity;
} FileList;

FileList *filelist_create(void);
void      filelist_add(FileList *list, const char *path);
void      filelist_free(FileList *list);

/*
 * Recursively enumerate files whose suffix matches one of the
 * supplied extensions (e.g. ".mp4").  Directories named in
 * exclude_dirs are skipped.
 */
void find_files_by_extensions(
    const char  *directory,
    const char **extensions,   int ext_count,
    const char **exclude_dirs, int excl_count,
    int          recursive,
    FileList    *result
);

/* ------------------------------------------------------------------ */
/*  ZIP central-directory reader                                        */
/*  Used by comanga, pageCounter (EPUB = ZIP, DOCX = ZIP).             */
/* ------------------------------------------------------------------ */
typedef struct {
    char filename[512];
} ZipEntry;

typedef struct {
    ZipEntry *entries;
    int       count;
} ZipFileList;

/*
 * Reads the ZIP central directory and fills *out with all entry
 * filenames.  Returns 1 on success, 0 on failure.
 * Caller must call zipfilelist_free() when done.
 */
int  zip_list_files(const char *zip_path, ZipFileList *out);
void zipfilelist_free(ZipFileList *list);

/* ------------------------------------------------------------------ */
/*  Progress reporter                                                   */
/* ------------------------------------------------------------------ */
typedef struct {
    int  total;
    int  current;
    char description[256];
} ProgressReporter;

void progress_init  (ProgressReporter *pr, int total, const char *desc);
void progress_update(ProgressReporter *pr, int increment);
void progress_finish(ProgressReporter *pr);

/* ------------------------------------------------------------------ */
/*  String helpers                                                      */
/* ------------------------------------------------------------------ */
/* Case-insensitive suffix check */
int str_ends_with_ci(const char *str, const char *suffix);

/* Case-insensitive search within a string (returns pointer or NULL) */
const char *str_stristr(const char *haystack, const char *needle);

#endif /* SHARED_UTILS_H */
