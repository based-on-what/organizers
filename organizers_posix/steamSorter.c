/*
 * steamSorter.c  (POSIX version)
 * Translated from: steamSorter.py
 *
 * Purpose: Steam Game Duration Analyzer.
 *
 * POSIX differences vs Windows version:
 *   - HTTP/HTTPS: libcurl instead of WinHTTP.
 *     curl_easy_perform handles both HTTP and HTTPS transparently.
 *   - JSON parsing: same minimal inline string scanner.
 *   - Everything else (logic, output) is identical.
 *
 * Dependencies:
 *   libcurl  (almost always pre-installed on Linux/macOS)
 *
 * Install libcurl dev headers if missing:
 *   Linux (Debian/Ubuntu):  sudo apt install libcurl4-openssl-dev
 *   Linux (Fedora):         sudo dnf install libcurl-devel
 *   macOS (Homebrew):       brew install curl
 *   macOS (system):         already included in Xcode Command Line Tools
 *
 * Compilation (Linux):
 *   gcc -std=c11 -Wall -O2 steamSorter.c shared_utils.c -o steamSorter \
 *       $(pkg-config --cflags --libs libcurl)
 * Compilation (macOS):
 *   clang -std=c11 -Wall -O2 steamSorter.c shared_utils.c -o steamSorter \
 *       $(pkg-config --cflags --libs libcurl)
 *   (or: ... -lcurl  if pkg-config is not available)
 */

#include "shared_utils.h"
#include <curl/curl.h>

/* ------------------------------------------------------------------ */
/*  libcurl response buffer                                             */
/* ------------------------------------------------------------------ */
typedef struct {
    char  *data;
    size_t size;
} CurlBuf;

static size_t curl_write_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    CurlBuf *buf = (CurlBuf *)userdata;
    size_t new_bytes = size * nmemb;
    buf->data = realloc(buf->data, buf->size + new_bytes + 1);
    if (!buf->data) return 0;
    memcpy(buf->data + buf->size, ptr, new_bytes);
    buf->size += new_bytes;
    buf->data[buf->size] = '\0';
    return new_bytes;
}

/* ------------------------------------------------------------------ */
/*  HTTP GET / POST via libcurl                                         */
/* ------------------------------------------------------------------ */
/*
 * Returns a malloc'd null-terminated response body, or NULL on failure.
 * post_body == NULL means GET.
 */
static char *curl_request(const char *url,
                           const char *post_body,
                           const char *content_type)
{
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    CurlBuf buf = { NULL, 0 };

    curl_easy_setopt(curl, CURLOPT_URL,           url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &buf);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,       30L);
    /* Accept modern TLS; let libcurl use system CA store */
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    struct curl_slist *headers = NULL;
    if (content_type) {
        headers = curl_slist_append(headers, content_type);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    if (post_body) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_body);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(post_body));
    }

    /* Browser-like User-Agent to avoid being blocked */
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
                     "Mozilla/5.0 (compatible; SteamSorter/1.0)");

    CURLcode res = curl_easy_perform(curl);

    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LOG_W("curl error: %s", curl_easy_strerror(res));
        free(buf.data);
        return NULL;
    }
    return buf.data; /* caller must free() */
}

/* ------------------------------------------------------------------ */
/*  Minimal JSON value extractor  (identical to Windows version)       */
/* ------------------------------------------------------------------ */
static int json_next_value(const char *json_str, size_t *pos,
                            const char *key,
                            char *value_buf, size_t value_buf_size)
{
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    size_t pat_len = strlen(pattern);

    const char *start = json_str + *pos;
    const char *found = str_stristr(start, pattern);
    if (!found) return 0;

    const char *p = found + pat_len;
    while (*p == ' ' || *p == '\t' || *p == ':') p++;

    if (*p == '"') {
        p++;
        size_t i = 0;
        while (*p && *p != '"' && i + 1 < value_buf_size) {
            if (*p == '\\') { p++; if (!*p) break; }
            value_buf[i++] = *p++;
        }
        value_buf[i] = '\0';
    } else if ((*p >= '0' && *p <= '9') || *p == '-' || *p == '.') {
        size_t i = 0;
        while ((*p>='0'&&*p<='9')||*p=='.'||*p=='-'||*p=='e'||*p=='E') {
            if (i + 1 < value_buf_size) value_buf[i++] = *p;
            p++;
        }
        value_buf[i] = '\0';
    } else { return 0; }

    *pos = (size_t)(p - json_str);
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Steam API: get game names                                           */
/* ------------------------------------------------------------------ */
static int steam_get_game_names(const char *api_key, const char *steam_id,
                                 char **names, int max_names)
{
    char url[1024];
    snprintf(url, sizeof(url),
             "http://api.steampowered.com/IPlayerService/GetOwnedGames/v0001/"
             "?key=%s&steamid=%s&format=json&include_appinfo=1",
             api_key, steam_id);

    char *body = curl_request(url, NULL, NULL);
    if (!body) { LOG_E("Failed to fetch Steam data for: %s", steam_id); return -1; }

    int    count = 0;
    size_t pos   = 0;
    char   value[512];

    const char *games_start = str_stristr(body, "\"games\"");
    if (!games_start) {
        LOG_W("No games data for Steam ID: %s", steam_id);
        free(body); return 0;
    }
    pos = (size_t)(games_start - body);

    while (count < max_names &&
           json_next_value(body, &pos, "name", value, sizeof(value)))
        names[count++] = strdup(value);

    free(body);
    LOG_I("Found %d game(s) for Steam ID: %s", count, steam_id);
    return count;
}

/* ------------------------------------------------------------------ */
/*  HLTB: get main-story completion time                               */
/* ------------------------------------------------------------------ */
static double hltb_get_main_story(const char *game_name)
{
    /* Escape double-quotes in the game name */
    char escaped[512]; const char *src = game_name; char *dst = escaped;
    while (*src && dst - escaped < (int)sizeof(escaped) - 2) {
        if (*src == '"') *dst++ = '\\';
        *dst++ = *src++;
    }
    *dst = '\0';

    char post_body[1024];
    snprintf(post_body, sizeof(post_body),
        "{\"searchType\":\"games\","
        "\"searchTerms\":[\"%s\"],"
        "\"searchPage\":1,\"size\":5,"
        "\"searchOptions\":{"
          "\"games\":{\"userId\":0,\"platform\":\"\","
          "\"sortCategory\":\"popular\","
          "\"rangeCategory\":\"main\","
          "\"rangeTime\":{\"min\":null,\"max\":null},"
          "\"gameplay\":{\"perspective\":\"\","
          "\"flow\":\"\",\"genre\":\"\","
          "\"subGenre\":\"\",\"difficulty\":\"\"},"
          "\"rangeYear\":{\"min\":\"\",\"max\":\"\"},"
          "\"modifier\":\"\"},"
        "\"lists\":{},\"users\":{\"sortCategory\":\"postcount\"},"
        "\"filter\":\"\",\"sort\":0,\"randomizer\":0}}",
        escaped);

    char *body = curl_request(
        "https://howlongtobeat.com/api/search/all",
        post_body,
        "Content-Type: application/json");

    if (!body) return -1.0;

    double hours = -1.0;
    size_t pos   = 0;
    char   value[64];

    if (json_next_value(body, &pos, "comp_main", value, sizeof(value))) {
        double s = atof(value);
        if (s > 0) hours = s / 3600.0;
    }
    if (hours < 0) {
        pos = 0;
        if (json_next_value(body, &pos, "gameplayMain", value, sizeof(value))) {
            double h = atof(value);
            if (h > 0) hours = h;
        }
    }
    free(body);
    return hours;
}

/* ------------------------------------------------------------------ */
/*  Result record and save                                              */
/* ------------------------------------------------------------------ */
typedef struct { char name[512]; double hours; } GameResult;
static GameResult g_results[4096]; static int g_result_count = 0;

static int game_cmp(const void *a, const void *b)
{
    double ha = ((const GameResult *)a)->hours;
    double hb = ((const GameResult *)b)->hours;
    return (ha > hb) - (ha < hb);
}

static void save_game_results(const char *output_file)
{
    int valid = 0;
    for (int i = 0; i < g_result_count; i++)
        if (g_results[i].hours >= 0) valid++;
    if (valid == 0) { LOG_W("No games had completion data."); return; }

    qsort(g_results, g_result_count, sizeof(GameResult), game_cmp);

    int    max_lines = g_result_count + 8;
    char **lines = malloc(max_lines * sizeof(char *));
    char  *bufs  = malloc(max_lines * 512);
    if (!lines || !bufs) { free(lines); free(bufs); return; }

    int    lc          = 0;
    double total_hours = 0;
#define LINE(fmt, ...) do { \
    lines[lc] = bufs + lc*512; snprintf(lines[lc],512,fmt,##__VA_ARGS__); lc++; } while(0)

    for (int i = 0; i < g_result_count; i++) {
        if (g_results[i].hours < 0) continue;
        LINE("%s: %.1f hours", g_results[i].name, g_results[i].hours);
        total_hours += g_results[i].hours;
    }
    LINE(""); LINE("Total games with data: %d",   valid);
    LINE("Total games processed: %d",  g_result_count);
    LINE("Games without data: %d",     g_result_count - valid);
    if (valid > 0) {
        LINE("Total time: %.1f hours",   total_hours);
        LINE("Average: %.1f hours",      total_hours / valid);
    }
#undef LINE
    save_results_to_file((const char **)lines, lc,
                          output_file, "STEAM GAMES BY COMPLETION TIME");
    free(lines); free(bufs);
}

/* ------------------------------------------------------------------ */
/*  Entry point                                                         */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    log_init(LOG_INFO, NULL);
    LOG_I("Steam Game Duration Analyzer");

    const char *api_key = "";
    const char *default_ids[] = { "", NULL };
    const char  *actual_key  = api_key;
    const char **steam_ids   = default_ids;
    int          id_count    = 0;

    if (argc >= 3) {
        actual_key = argv[1];
        steam_ids  = (const char **)(argv + 2);
        id_count   = argc - 2;
    } else {
        for (int i = 0; default_ids[i]; i++)
            if (default_ids[i][0] != '\0') id_count++;
    }

    if (!actual_key || actual_key[0] == '\0') {
        LOG_E("Steam API key required. Pass as first argument.");
        LOG_E("  Usage: %s <API_KEY> <STEAM_ID> [STEAM_ID2 ...]", argv[0]);
        return 1;
    }
    if (id_count == 0) {
        LOG_E("At least one Steam ID required."); return 1;
    }

    /* Initialize libcurl once */
    curl_global_init(CURL_GLOBAL_DEFAULT);

    /* Collect all unique game names */
    char *all_names[8192]; int all_count = 0;
    for (int i = 0; i < id_count; i++) {
        if (!steam_ids[i] || steam_ids[i][0] == '\0') continue;
        char *batch[2048];
        int n = steam_get_game_names(actual_key, steam_ids[i], batch, 2048);
        if (n <= 0) continue;
        for (int j = 0; j < n; j++) {
            int dup = 0;
            for (int k = 0; k < all_count; k++)
                if (strcasecmp(all_names[k], batch[j]) == 0) { dup=1; break; }
            if (!dup && all_count < 8192) all_names[all_count++] = batch[j];
            else                           free(batch[j]);
        }
    }

    LOG_I("Total unique games: %d. Looking up HLTB...", all_count);

    ProgressReporter pr; progress_init(&pr, all_count, "HLTB lookup");
    for (int i = 0; i < all_count; i++) {
        double h = hltb_get_main_story(all_names[i]);
        if (h >= 0) LOG_I("OK  %s: %.1f hours", all_names[i], h);
        else        LOG_W("N/A %s", all_names[i]);
        if (g_result_count < 4096) {
            snprintf(g_results[g_result_count].name, 512, "%s", all_names[i]);
            g_results[g_result_count++].hours = h;
        }
        free(all_names[i]);
        progress_update(&pr, 1);
    }
    progress_finish(&pr);

    curl_global_cleanup();

    int with_data = 0;
    for (int i = 0; i < g_result_count; i++) if (g_results[i].hours >= 0) with_data++;
    LOG_I("Total analyzed: %d  |  With data: %d  |  Without: %d",
          g_result_count, with_data, g_result_count - with_data);

    save_game_results("steam_games_completion_times.txt");
    LOG_I("Done. Results saved to steam_games_completion_times.txt");
    log_close();
    return 0;
}
