/*
 * shared_utils.h  (POSIX version)
 * Portable equivalent of organizers_c/shared_utils.h
 *
 * Targets: Linux, macOS (any POSIX-compliant system)
 * Compiler: gcc / clang  (-std=c11)
 *
 * Win32 → POSIX substitutions used here:
 *   GetLocalTime          → time() + localtime()
 *   FindFirstFile/Next    → opendir() / readdir()   <dirent.h>
 *   GetFileAttributesExA  → stat()                  <sys/stat.h>
 *   CreateFile + ReadFile → fopen() + fread()
 *   MAX_PATH              → PATH_MAX                <limits.h>
 *   _strdup               → strdup
 *   _stricmp              → strcasecmp              <strings.h>
 *   _strnicmp             → strncasecmp
 *   _snprintf             → snprintf  (standard C99)
 *   _vsnprintf            → vsnprintf (standard C99)
 */

#ifndef SHARED_UTILS_H
#define SHARED_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>    /* strcasecmp, strncasecmp */
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <stdarg.h>

/* Use system PATH_MAX, fall back to 4096 */
#ifndef PATH_MAX
#  define PATH_MAX 4096
#endif

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
/*  File validation                                                     */
/* ------------------------------------------------------------------ */
int validate_file_path(const char *path, long long min_size);
int safe_file_operation(const char *path);

/* ------------------------------------------------------------------ */
/*  Result writer                                                        */
/* ------------------------------------------------------------------ */
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

void find_files_by_extensions(
    const char  *directory,
    const char **extensions,   int ext_count,
    const char **exclude_dirs, int excl_count,
    int          recursive,
    FileList    *result
);

/* ------------------------------------------------------------------ */
/*  ZIP central-directory reader (identical to Windows version —       */
/*  ZIP is a portable binary format)                                   */
/* ------------------------------------------------------------------ */
typedef struct { char filename[512]; } ZipEntry;
typedef struct { ZipEntry *entries; int count; } ZipFileList;

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
int         str_ends_with_ci(const char *str, const char *suffix);
const char *str_stristr     (const char *haystack, const char *needle);

#endif /* SHARED_UTILS_H */
