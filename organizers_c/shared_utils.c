/*
 * shared_utils.c
 * Translated from: shared_utils.py
 *
 * Purpose: Windows-native implementations of common utilities used by
 *          all organizer tools in this collection.
 *
 * Compilation (MSVC):
 *   cl /W3 /std:c11 shared_utils.c
 * Compilation (MinGW):
 *   gcc -std=c11 -Wall shared_utils.c -o shared_utils.o -c
 */

#include "shared_utils.h"
#include <stdarg.h>

/* ------------------------------------------------------------------ */
/*  Globals                                                             */
/* ------------------------------------------------------------------ */
int   g_log_level = LOG_INFO;
FILE *g_log_file  = NULL;

static const char *s_level_names[] = { "DEBUG", "INFO", "WARNING", "ERROR" };

/* ------------------------------------------------------------------ */
/*  Logging                                                             */
/* ------------------------------------------------------------------ */
void log_init(int level, const char *log_file_path)
{
    g_log_level = level;
    if (log_file_path && log_file_path[0] != '\0') {
        g_log_file = fopen(log_file_path, "a");
        if (!g_log_file)
            fprintf(stderr, "[WARNING] Could not open log file: %s\n", log_file_path);
    }
}

void log_msg(int level, const char *fmt, ...)
{
    if (level < g_log_level)
        return;

    /* Timestamp via Win32 GetLocalTime */
    SYSTEMTIME st;
    GetLocalTime(&st);

    char timestamp[32];
    _snprintf(timestamp, sizeof(timestamp),
              "%04d-%02d-%02d %02d:%02d:%02d",
              st.wYear, st.wMonth, st.wDay,
              st.wHour, st.wMinute, st.wSecond);

    va_list args;
    va_start(args, fmt);
    char msg[4096];
    _vsnprintf(msg, sizeof(msg) - 1, fmt, args);
    msg[sizeof(msg) - 1] = '\0';
    va_end(args);

    const char *lname = (level >= 0 && level <= LOG_ERROR)
                        ? s_level_names[level] : "UNKNOWN";

    printf("%s - %s - %s\n", timestamp, lname, msg);
    fflush(stdout);

    if (g_log_file) {
        fprintf(g_log_file, "%s - %s - %s\n", timestamp, lname, msg);
        fflush(g_log_file);
    }
}

void log_close(void)
{
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
}

/* ------------------------------------------------------------------ */
/*  Formatting helpers                                                  */
/* ------------------------------------------------------------------ */
void format_file_size(char *buf, size_t buf_size, long long size_bytes)
{
    static const char *units[] = { "B", "KB", "MB", "GB", "TB" };
    double size = (double)size_bytes;
    int i = 0;
    while (size >= 1024.0 && i < 4) {
        size /= 1024.0;
        i++;
    }
    _snprintf(buf, buf_size, "%.1f %s", size, units[i]);
}

void format_duration(char *buf, size_t buf_size, double seconds)
{
    int h = (int)(seconds / 3600.0);
    int m = (int)((seconds - h * 3600.0) / 60.0);
    int s = (int)(seconds - h * 3600.0 - m * 60.0);
    _snprintf(buf, buf_size, "%02d:%02d:%02d", h, m, s);
}

/* ------------------------------------------------------------------ */
/*  File validation                                                     */
/* ------------------------------------------------------------------ */
int validate_file_path(const char *path, long long min_size)
{
    wchar_t wpath[MAX_PATH];
    utf8_to_wide(path, wpath, MAX_PATH);
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(wpath, GetFileExInfoStandard, &fad))
        return 0;
    if (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        return 0;

    LARGE_INTEGER sz;
    sz.HighPart = (LONG)fad.nFileSizeHigh;
    sz.LowPart  = fad.nFileSizeLow;
    return sz.QuadPart >= min_size;
}

int safe_file_operation(const char *path)
{
    wchar_t wpath[MAX_PATH];
    utf8_to_wide(path, wpath, MAX_PATH);
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(wpath, GetFileExInfoStandard, &fad)) {
        LOG_W("File does not exist: %s", path);
        return 0;
    }
    if (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        LOG_W("Path is not a file: %s", path);
        return 0;
    }
    LARGE_INTEGER sz;
    sz.HighPart = (LONG)fad.nFileSizeHigh;
    sz.LowPart  = fad.nFileSizeLow;
    if (sz.QuadPart == 0) {
        LOG_W("File is empty: %s", path);
        return 0;
    }
    /* Try to open for reading */
    HANDLE h = CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        LOG_E("Permission denied or OS error accessing: %s", path);
        return 0;
    }
    CloseHandle(h);
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Result writer                                                       */
/* ------------------------------------------------------------------ */
int save_results_to_file(const char **lines, int line_count,
                          const char *output_file, const char *title)
{
    wchar_t wout[MAX_PATH];
    utf8_to_wide(output_file, wout, MAX_PATH);
    FILE *f = _wfopen(wout, L"wb");   /* binary so we control CRLF exactly */
    if (!f) {
        LOG_E("Cannot open output file: %s", output_file);
        return 0;
    }

    /* UTF-8 BOM — makes the file recognisable as UTF-8 in Notepad/editors */
    unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
    fwrite(bom, 1, 3, f);

    int title_len = (int)strlen(title);
    fprintf(f, "%s\r\n", title);
    for (int i = 0; i < title_len; i++)
        fputc('=', f);
    fprintf(f, "\r\n\r\n");

    for (int i = 0; i < line_count; i++)
        fprintf(f, "%s\r\n", lines[i] ? lines[i] : "");

    fclose(f);
    LOG_I("Results saved to: %s", output_file);
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Dynamic file list                                                   */
/* ------------------------------------------------------------------ */
FileList *filelist_create(void)
{
    FileList *list = (FileList *)malloc(sizeof(FileList));
    if (!list) return NULL;
    list->count    = 0;
    list->capacity = 64;
    list->paths    = (char **)malloc(list->capacity * sizeof(char *));
    if (!list->paths) { free(list); return NULL; }
    return list;
}

void filelist_add(FileList *list, const char *path)
{
    if (!list || !path) return;
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        list->paths = (char **)realloc(list->paths,
                                        list->capacity * sizeof(char *));
    }
    list->paths[list->count++] = _strdup(path);
}

void filelist_free(FileList *list)
{
    if (!list) return;
    for (int i = 0; i < list->count; i++)
        free(list->paths[i]);
    free(list->paths);
    free(list);
}

/* ------------------------------------------------------------------ */
/*  String helpers                                                      */
/* ------------------------------------------------------------------ */
int str_ends_with_ci(const char *str, const char *suffix)
{
    size_t sl = strlen(str);
    size_t xl = strlen(suffix);
    if (xl > sl) return 0;
    const char *s = str + sl - xl;
    const char *x = suffix;
    while (*x) {
        if (tolower((unsigned char)*s) != tolower((unsigned char)*x))
            return 0;
        s++; x++;
    }
    return 1;
}

const char *str_stristr(const char *haystack, const char *needle)
{
    if (!*needle) return haystack;
    size_t nl = strlen(needle);
    for (; *haystack; haystack++) {
        if (_strnicmp(haystack, needle, nl) == 0)
            return haystack;
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  UTF-8 / UTF-16 conversion helpers                                  */
/* ------------------------------------------------------------------ */
void utf8_to_wide(const char *utf8, wchar_t *wide, int wide_chars)
{
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, wide_chars);
}

void wide_to_utf8(const wchar_t *wide, char *utf8, int utf8_bytes)
{
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, utf8_bytes, NULL, NULL);
}

/* ------------------------------------------------------------------ */
/*  Recursive file finder (FindFirstFileW / FindNextFileW)             */
/* ------------------------------------------------------------------ */
static int is_in_list_ci(const char *name, const char **list, int count)
{
    for (int i = 0; i < count; i++)
        if (_stricmp(name, list[i]) == 0)
            return 1;
    return 0;
}

void find_files_by_extensions(
    const char  *directory,
    const char **extensions,   int ext_count,
    const char **exclude_dirs, int excl_count,
    int          recursive,
    FileList    *result)
{
    wchar_t wdir[MAX_PATH], wpattern[MAX_PATH];
    utf8_to_wide(directory, wdir, MAX_PATH);
    _snwprintf(wpattern, MAX_PATH, L"%s\\*", wdir);

    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(wpattern, &ffd);
    if (hFind == INVALID_HANDLE_VALUE)
        return;

    do {
        if (wcscmp(ffd.cFileName, L".") == 0 ||
            wcscmp(ffd.cFileName, L"..") == 0)
            continue;

        /* Convert the wide filename to UTF-8 for comparisons and storage */
        char fname_u8[MAX_PATH * 3];
        wide_to_utf8(ffd.cFileName, fname_u8, sizeof(fname_u8));

        char full_path[MAX_PATH * 3];
        _snprintf(full_path, sizeof(full_path), "%s\\%s", directory, fname_u8);

        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (recursive &&
                !is_in_list_ci(fname_u8, exclude_dirs, excl_count))
            {
                find_files_by_extensions(full_path,
                                         extensions, ext_count,
                                         exclude_dirs, excl_count,
                                         recursive, result);
            }
        } else {
            for (int i = 0; i < ext_count; i++) {
                if (str_ends_with_ci(fname_u8, extensions[i])) {
                    filelist_add(result, full_path);
                    break;
                }
            }
        }
    } while (FindNextFileW(hFind, &ffd));

    FindClose(hFind);
}

/* ------------------------------------------------------------------ */
/*  ZIP central-directory reader                                        */
/* ------------------------------------------------------------------ */

/* ZIP structure field offsets (little-endian) */
#define EOCD_SIGNATURE      0x06054b50u
#define CD_ENTRY_SIGNATURE  0x02014b50u
#define MAX_EOCD_SEARCH     65536u   /* max comment length + EOCD size */

static unsigned int read_u32_le(const unsigned char *p)
{
    return (unsigned int)(p[0]) |
           ((unsigned int)(p[1]) << 8) |
           ((unsigned int)(p[2]) << 16) |
           ((unsigned int)(p[3]) << 24);
}

static unsigned short read_u16_le(const unsigned char *p)
{
    return (unsigned short)(p[0]) |
           ((unsigned short)(p[1]) << 8);
}

int zip_list_files(const char *zip_path, ZipFileList *out)
{
    out->entries = NULL;
    out->count   = 0;

    wchar_t wzip[MAX_PATH];
    utf8_to_wide(zip_path, wzip, MAX_PATH);
    HANDLE hFile = CreateFileW(wzip, GENERIC_READ, FILE_SHARE_READ,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return 0;

    /* --- Find EOCD record by searching backwards from end of file --- */
    LARGE_INTEGER file_size_li;
    if (!GetFileSizeEx(hFile, &file_size_li)) { CloseHandle(hFile); return 0; }
    long long file_size = file_size_li.QuadPart;

    /* Read last MAX_EOCD_SEARCH bytes into a buffer */
    long long search_start = file_size - MAX_EOCD_SEARCH;
    if (search_start < 0) search_start = 0;
    DWORD buf_size = (DWORD)(file_size - search_start);

    unsigned char *buf = (unsigned char *)malloc(buf_size);
    if (!buf) { CloseHandle(hFile); return 0; }

    LARGE_INTEGER offset_li;
    offset_li.QuadPart = search_start;
    SetFilePointerEx(hFile, offset_li, NULL, FILE_BEGIN);
    DWORD bytes_read;
    ReadFile(hFile, buf, buf_size, &bytes_read, NULL);

    /* Search backwards for EOCD signature */
    int eocd_pos = -1;
    for (int i = (int)bytes_read - 22; i >= 0; i--) {
        if (read_u32_le(buf + i) == EOCD_SIGNATURE) {
            eocd_pos = i;
            break;
        }
    }
    if (eocd_pos < 0) { free(buf); CloseHandle(hFile); return 0; }

    const unsigned char *eocd = buf + eocd_pos;
    unsigned int  cd_size   = read_u32_le(eocd + 12);
    unsigned int  cd_offset = read_u32_le(eocd + 16);
    unsigned short num_entries = read_u16_le(eocd + 10);
    free(buf);

    /* --- Read central directory --- */
    unsigned char *cd_buf = (unsigned char *)malloc(cd_size + 1);
    if (!cd_buf) { CloseHandle(hFile); return 0; }

    offset_li.QuadPart = cd_offset;
    SetFilePointerEx(hFile, offset_li, NULL, FILE_BEGIN);
    ReadFile(hFile, cd_buf, cd_size, &bytes_read, NULL);
    CloseHandle(hFile);

    /* Allocate entry array */
    out->entries = (ZipEntry *)malloc(num_entries * sizeof(ZipEntry));
    if (!out->entries) { free(cd_buf); return 0; }
    out->count = 0;

    unsigned char *p = cd_buf;
    unsigned char *end = cd_buf + bytes_read;

    for (int i = 0; i < num_entries && p + 46 <= end; i++) {
        if (read_u32_le(p) != CD_ENTRY_SIGNATURE) break;

        unsigned short fname_len   = read_u16_le(p + 28);
        unsigned short extra_len   = read_u16_le(p + 30);
        unsigned short comment_len = read_u16_le(p + 32);

        if (p + 46 + fname_len > end) break;

        /* Copy filename (truncate to buffer size) */
        unsigned short copy_len = fname_len;
        if (copy_len >= (unsigned short)(sizeof(out->entries[0].filename)))
            copy_len = (unsigned short)(sizeof(out->entries[0].filename)) - 1;

        memcpy(out->entries[out->count].filename, p + 46, copy_len);
        out->entries[out->count].filename[copy_len] = '\0';
        out->count++;

        p += 46 + fname_len + extra_len + comment_len;
    }

    free(cd_buf);
    return 1;
}

void zipfilelist_free(ZipFileList *list)
{
    if (list && list->entries) {
        free(list->entries);
        list->entries = NULL;
        list->count   = 0;
    }
}

/* ------------------------------------------------------------------ */
/*  Progress reporter                                                   */
/* ------------------------------------------------------------------ */
void progress_init(ProgressReporter *pr, int total, const char *desc)
{
    pr->total   = total;
    pr->current = 0;
    _snprintf(pr->description, sizeof(pr->description) - 1, "%s", desc);
}

void progress_update(ProgressReporter *pr, int increment)
{
    pr->current += increment;
    double pct = pr->total > 0
                 ? (double)pr->current / pr->total * 100.0
                 : 0.0;
    LOG_I("%s: %d/%d (%.1f%%)", pr->description,
          pr->current, pr->total, pct);
}

void progress_finish(ProgressReporter *pr)
{
    LOG_I("%s completed: %d/%d",
          pr->description, pr->current, pr->total);
}

char *escape_windows_arg(const char *arg)
{
    if (!arg) return NULL;
    size_t len = strlen(arg);
    char *buf = malloc(len * 2 + 1);
    if (!buf) return NULL;
    char *dst = buf;
    for (const char *src = arg; *src; src++) {
        if (*src == '"') *dst++ = '"';
        *dst++ = *src;
    }
    *dst = '\0';
    return buf;
}
