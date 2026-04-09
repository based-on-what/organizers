/*
 * shared_utils.c  (POSIX version)
 *
 * Compilation (Linux):
 *   gcc -std=c11 -Wall -c shared_utils.c -o shared_utils.o
 * Compilation (macOS):
 *   clang -std=c11 -Wall -c shared_utils.c -o shared_utils.o
 */

#include "shared_utils.h"

/* ------------------------------------------------------------------ */
/*  Globals                                                             */
/* ------------------------------------------------------------------ */
int   g_log_level = LOG_INFO;
FILE *g_log_file  = NULL;

static const char *s_level_names[] = { "DEBUG", "INFO", "WARNING", "ERROR" };

/* ------------------------------------------------------------------ */
/*  Logging  (time() + localtime() instead of GetLocalTime)            */
/* ------------------------------------------------------------------ */
void log_init(int level, const char *log_file_path)
{
    g_log_level = level;
    if (log_file_path && log_file_path[0] != '\0') {
        g_log_file = fopen(log_file_path, "a");
        if (!g_log_file)
            fprintf(stderr, "[WARNING] Could not open log file: %s\n",
                    log_file_path);
    }
}

void log_msg(int level, const char *fmt, ...)
{
    if (level < g_log_level) return;

    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    va_list args;
    va_start(args, fmt);
    char msg[4096];
    vsnprintf(msg, sizeof(msg) - 1, fmt, args);
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
    if (g_log_file) { fclose(g_log_file); g_log_file = NULL; }
}

/* ------------------------------------------------------------------ */
/*  Formatting helpers  (pure C, identical to Windows version)         */
/* ------------------------------------------------------------------ */
void format_file_size(char *buf, size_t buf_size, long long size_bytes)
{
    static const char *units[] = { "B", "KB", "MB", "GB", "TB" };
    double size = (double)size_bytes;
    int i = 0;
    while (size >= 1024.0 && i < 4) { size /= 1024.0; i++; }
    snprintf(buf, buf_size, "%.1f %s", size, units[i]);
}

void format_duration(char *buf, size_t buf_size, double seconds)
{
    int h = (int)(seconds / 3600.0);
    int m = (int)((seconds - h * 3600.0) / 60.0);
    int s = (int)(seconds - h * 3600.0 - m * 60.0);
    snprintf(buf, buf_size, "%02d:%02d:%02d", h, m, s);
}

/* ------------------------------------------------------------------ */
/*  File validation  (stat() instead of GetFileAttributesExA)          */
/* ------------------------------------------------------------------ */
int validate_file_path(const char *path, long long min_size)
{
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    if (!S_ISREG(st.st_mode)) return 0;
    return (long long)st.st_size >= min_size;
}

int safe_file_operation(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        LOG_W("File does not exist: %s", path);
        return 0;
    }
    if (!S_ISREG(st.st_mode)) {
        LOG_W("Path is not a regular file: %s", path);
        return 0;
    }
    if (st.st_size == 0) {
        LOG_W("File is empty: %s", path);
        return 0;
    }
    /* Try to open for reading */
    FILE *f = fopen(path, "rb");
    if (!f) {
        LOG_E("Permission denied or error accessing: %s", path);
        return 0;
    }
    fclose(f);
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Result writer                                                       */
/* ------------------------------------------------------------------ */
int save_results_to_file(const char **lines, int line_count,
                          const char *output_file, const char *title)
{
    FILE *f = fopen(output_file, "w");
    if (!f) { LOG_E("Cannot open output file: %s", output_file); return 0; }

    int tlen = (int)strlen(title);
    fprintf(f, "%s\n", title);
    for (int i = 0; i < tlen; i++) fputc('=', f);
    fprintf(f, "\n\n");
    for (int i = 0; i < line_count; i++)
        fprintf(f, "%s\n", lines[i] ? lines[i] : "");
    fclose(f);
    LOG_I("Results saved to: %s", output_file);
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Dynamic file list                                                   */
/* ------------------------------------------------------------------ */
FileList *filelist_create(void)
{
    FileList *list = malloc(sizeof(FileList));
    if (!list) return NULL;
    list->count    = 0;
    list->capacity = 64;
    list->paths    = malloc(list->capacity * sizeof(char *));
    if (!list->paths) { free(list); return NULL; }
    return list;
}

void filelist_add(FileList *list, const char *path)
{
    if (!list || !path) return;
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        list->paths = realloc(list->paths, list->capacity * sizeof(char *));
    }
    list->paths[list->count++] = strdup(path);
}

void filelist_free(FileList *list)
{
    if (!list) return;
    for (int i = 0; i < list->count; i++) free(list->paths[i]);
    free(list->paths);
    free(list);
}

/* ------------------------------------------------------------------ */
/*  String helpers                                                      */
/* ------------------------------------------------------------------ */
int str_ends_with_ci(const char *str, const char *suffix)
{
    size_t sl = strlen(str), xl = strlen(suffix);
    if (xl > sl) return 0;
    return strcasecmp(str + sl - xl, suffix) == 0;
}

const char *str_stristr(const char *haystack, const char *needle)
{
    if (!*needle) return haystack;
    size_t nl = strlen(needle);
    for (; *haystack; haystack++)
        if (strncasecmp(haystack, needle, nl) == 0)
            return haystack;
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Recursive file finder  (opendir/readdir instead of FindFirstFile)  */
/* ------------------------------------------------------------------ */
static int is_in_list_ci(const char *name, const char **list, int count)
{
    for (int i = 0; i < count; i++)
        if (strcasecmp(name, list[i]) == 0) return 1;
    return 0;
}

void find_files_by_extensions(
    const char  *directory,
    const char **extensions,   int ext_count,
    const char **exclude_dirs, int excl_count,
    int          recursive,
    FileList    *result)
{
    DIR *dir = opendir(directory);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) continue;

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s",
                 directory, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            if (recursive &&
                !is_in_list_ci(entry->d_name, exclude_dirs, excl_count))
                find_files_by_extensions(full_path,
                                         extensions, ext_count,
                                         exclude_dirs, excl_count,
                                         recursive, result);
        } else if (S_ISREG(st.st_mode)) {
            for (int i = 0; i < ext_count; i++)
                if (str_ends_with_ci(entry->d_name, extensions[i])) {
                    filelist_add(result, full_path);
                    break;
                }
        }
    }
    closedir(dir);
}

/* ------------------------------------------------------------------ */
/*  ZIP central-directory reader  (identical binary logic)             */
/* ------------------------------------------------------------------ */
#define EOCD_SIGNATURE     0x06054b50u
#define CD_ENTRY_SIGNATURE 0x02014b50u
#define MAX_EOCD_SEARCH    65536u

static unsigned int read_u32_le(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1]<<8)
         | ((unsigned int)p[2]<<16) | ((unsigned int)p[3]<<24);
}
static unsigned short read_u16_le(const unsigned char *p)
{
    return (unsigned short)(p[0] | (p[1]<<8));
}

int zip_list_files(const char *zip_path, ZipFileList *out)
{
    out->entries = NULL; out->count = 0;

    FILE *f = fopen(zip_path, "rb");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);

    long search_start = file_size - (long)MAX_EOCD_SEARCH;
    if (search_start < 0) search_start = 0;
    unsigned int buf_size = (unsigned int)(file_size - search_start);

    unsigned char *buf = malloc(buf_size);
    if (!buf) { fclose(f); return 0; }

    fseek(f, search_start, SEEK_SET);
    unsigned int bytes_read = (unsigned int)fread(buf, 1, buf_size, f);

    int eocd_pos = -1;
    for (int i = (int)bytes_read - 22; i >= 0; i--)
        if (read_u32_le(buf + i) == EOCD_SIGNATURE) { eocd_pos = i; break; }
    if (eocd_pos < 0) { free(buf); fclose(f); return 0; }

    const unsigned char *eocd = buf + eocd_pos;
    unsigned int  cd_size     = read_u32_le(eocd + 12);
    unsigned int  cd_offset   = read_u32_le(eocd + 16);
    unsigned short num_entries = read_u16_le(eocd + 10);
    free(buf);

    unsigned char *cd_buf = malloc(cd_size + 1);
    if (!cd_buf) { fclose(f); return 0; }
    fseek(f, (long)cd_offset, SEEK_SET);
    unsigned int cd_read = (unsigned int)fread(cd_buf, 1, cd_size, f);
    fclose(f);

    out->entries = malloc(num_entries * sizeof(ZipEntry));
    if (!out->entries) { free(cd_buf); return 0; }
    out->count = 0;

    unsigned char *p = cd_buf, *end = cd_buf + cd_read;
    for (int i = 0; i < num_entries && p + 46 <= end; i++) {
        if (read_u32_le(p) != CD_ENTRY_SIGNATURE) break;
        unsigned short fname_len   = read_u16_le(p + 28);
        unsigned short extra_len   = read_u16_le(p + 30);
        unsigned short comment_len = read_u16_le(p + 32);
        if (p + 46 + fname_len > end) break;
        unsigned short copy_len = fname_len;
        if (copy_len >= (unsigned short)sizeof(out->entries[0].filename))
            copy_len = (unsigned short)sizeof(out->entries[0].filename) - 1;
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
    pr->total = total; pr->current = 0;
    snprintf(pr->description, sizeof(pr->description) - 1, "%s", desc);
}
void progress_update(ProgressReporter *pr, int inc)
{
    pr->current += inc;
    double pct = pr->total > 0 ? (double)pr->current/pr->total*100.0 : 0.0;
    LOG_I("%s: %d/%d (%.1f%%)", pr->description,
          pr->current, pr->total, pct);
}
void progress_finish(ProgressReporter *pr)
{
    LOG_I("%s completed: %d/%d",
          pr->description, pr->current, pr->total);
}
