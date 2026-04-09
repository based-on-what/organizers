/*
 * steamSorter.c
 * Translated from: steamSorter.py
 *
 * Purpose: Steam Game Duration Analyzer.
 *   Fetches a user's Steam game library via the Steam Web API and
 *   looks up each game's main-story completion time from the
 *   HowLongToBeat (HLTB) API.  Results are sorted by completion
 *   time and saved to "steam_games_completion_times.txt".
 *
 * Windows-native APIs used:
 *   - WinHTTP (WinHttpOpen, WinHttpConnect, WinHttpOpenRequest,
 *     WinHttpSendRequest, WinHttpReceiveResponse, WinHttpReadData)
 *     for both HTTP (Steam) and HTTPS (HLTB) requests.
 *   - No third-party JSON library: a minimal string scanner is used
 *     to extract "name" fields from the Steam JSON response, and
 *     "comp_main" / "gameplayMain" values from the HLTB JSON response.
 *
 * Configuration:
 *   Edit the STEAM_API_KEY and STEAM_IDS arrays at the top of main().
 *   Alternatively, pass them as command-line arguments:
 *     steamSorter.exe <API_KEY> <STEAM_ID1> [STEAM_ID2 ...]
 *
 * Link libraries (MSVC):
 *   winhttp.lib
 * Compilation (MSVC):
 *   cl /W3 /std:c11 steamSorter.c shared_utils.c /Fe:steamSorter.exe ^
 *      winhttp.lib
 * Compilation (MinGW):
 *   gcc -std=c11 -Wall steamSorter.c shared_utils.c -o steamSorter.exe ^
 *       -lwinhttp
 *
 * Notes:
 *   - The HLTB search API (howlongtobeat.com) is unofficial and may
 *     change without notice.  If requests fail, the game is reported
 *     as having no completion data.
 *   - Steam API key must be obtained from:
 *     https://steamcommunity.com/dev/apikey
 */

#include "shared_utils.h"
#include <winhttp.h>

/* ------------------------------------------------------------------ */
/*  HTTP helper: perform a GET or POST request, return body as string  */
/* ------------------------------------------------------------------ */
/*
 * Returns a malloc'd null-terminated string containing the full
 * response body, or NULL on failure.  Caller must free().
 *
 * host     : L"api.steampowered.com"
 * path     : L"/IPlayerService/GetOwnedGames/..."
 * use_https: 1 for HTTPS, 0 for HTTP
 * post_body: NULL for GET; pointer to UTF-8 body string for POST
 * extra_headers: optional additional request headers (may be NULL)
 */
static char *http_request(const wchar_t *host, const wchar_t *path,
                           int use_https,
                           const char *post_body,
                           const wchar_t *content_type)
{
    HINTERNET hSession = WinHttpOpen(
        L"SteamSorter/1.0 (Windows; WinHTTP)",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return NULL;

    INTERNET_PORT port = use_https
                         ? INTERNET_DEFAULT_HTTPS_PORT
                         : INTERNET_DEFAULT_HTTP_PORT;

    HINTERNET hConnect = WinHttpConnect(hSession, host, port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return NULL; }

    DWORD req_flags = use_https ? WINHTTP_FLAG_SECURE : 0;
    const wchar_t *method = post_body ? L"POST" : L"GET";

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, method, path,
        NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        req_flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return NULL;
    }

    /* Add Content-Type header for POST */
    if (content_type)
        WinHttpAddRequestHeaders(hRequest, content_type, (DWORD)-1L,
                                  WINHTTP_ADDREQ_FLAG_ADD);

    /* Send the request */
    DWORD body_len = post_body ? (DWORD)strlen(post_body) : 0;
    BOOL ok = WinHttpSendRequest(hRequest,
                                  WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                  (LPVOID)post_body, body_len,
                                  body_len, 0);
    if (!ok || !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return NULL;
    }

    /* Read response body */
    char   *body      = NULL;
    size_t  body_size = 0;
    DWORD   avail     = 0;
    DWORD   read_now  = 0;
    char    chunk[8192];

    while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
        DWORD to_read = avail < sizeof(chunk) ? avail : (DWORD)sizeof(chunk);
        if (!WinHttpReadData(hRequest, chunk, to_read, &read_now))
            break;
        body = (char *)realloc(body, body_size + read_now + 1);
        if (!body) break;
        memcpy(body + body_size, chunk, read_now);
        body_size += read_now;
        body[body_size] = '\0';
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return body; /* caller must free() */
}

/* ------------------------------------------------------------------ */
/*  Minimal JSON string extractor                                       */
/* ------------------------------------------------------------------ */
/*
 * Finds the next occurrence of "key":"value" or "key": value (number)
 * in json_str starting at *pos.
 * If found, copies the value into value_buf (max value_buf_size chars)
 * and advances *pos past the match.
 * Returns 1 if found, 0 if not found.
 *
 * Supports only:
 *   - String values  (between double quotes, no escape handling)
 *   - Numeric values (digits and decimal point)
 */
static int json_next_value(const char *json_str, size_t *pos,
                            const char *key,
                            char *value_buf, size_t value_buf_size)
{
    /* Build search pattern:  "key": */
    char pattern[256];
    _snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    size_t pat_len = strlen(pattern);

    const char *start = json_str + *pos;
    const char *found = str_stristr(start, pattern);
    if (!found) return 0;

    const char *p = found + pat_len;
    /* Skip whitespace and colon */
    while (*p == ' ' || *p == '\t' || *p == ':' || *p == ' ') p++;

    if (*p == '"') {
        /* String value */
        p++; /* skip opening quote */
        size_t i = 0;
        while (*p && *p != '"' && i + 1 < value_buf_size) {
            /* Basic escape: skip \<char> pairs */
            if (*p == '\\') { p++; if (!*p) break; }
            value_buf[i++] = *p++;
        }
        value_buf[i] = '\0';
    } else if ((*p >= '0' && *p <= '9') || *p == '-' || *p == '.') {
        /* Numeric value */
        size_t i = 0;
        while ((*p >= '0' && *p <= '9') || *p == '.' || *p == '-' || *p == 'e' || *p == 'E') {
            if (i + 1 < value_buf_size)
                value_buf[i++] = *p;
            p++;
        }
        value_buf[i] = '\0';
    } else {
        return 0;
    }

    *pos = (size_t)(p - json_str);
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Steam API: get owned games for a user                               */
/* ------------------------------------------------------------------ */
/*
 * Fetches game names from:
 *   http://api.steampowered.com/IPlayerService/GetOwnedGames/v0001/
 *       ?key=KEY&steamid=ID&format=json&include_appinfo=1
 *
 * Fills names[] with up to max_names game name strings (malloc'd).
 * Returns number of games added, or -1 on error.
 * Caller must free() each returned name.
 */
static int steam_get_game_names(const char *api_key, const char *steam_id,
                                 char **names, int max_names)
{
    /* Build path */
    wchar_t path[1024];
    char key_copy[256], id_copy[64];
    _snprintf(key_copy, sizeof(key_copy), "%s", api_key);
    _snprintf(id_copy,  sizeof(id_copy),  "%s", steam_id);

    /* Convert to wide */
    wchar_t w_key[256], w_id[64];
    MultiByteToWideChar(CP_ACP, 0, key_copy, -1, w_key, 256);
    MultiByteToWideChar(CP_ACP, 0, id_copy,  -1, w_id,  64);

    /* URL-safe: steam API key contains only alphanumeric chars */
    swprintf(path, 1024,
             L"/IPlayerService/GetOwnedGames/v0001/"
             L"?key=%s&steamid=%s&format=json&include_appinfo=1",
             w_key, w_id);

    char *body = http_request(L"api.steampowered.com", path, 0, NULL, NULL);
    if (!body) {
        LOG_E("Failed to fetch Steam data for ID: %s", steam_id);
        return -1;
    }

    /* Parse: find all "name":"<value>" inside the "games" array */
    int count = 0;
    size_t pos = 0;
    char value[512];

    /* Skip to the "games" array */
    const char *games_start = str_stristr(body, "\"games\"");
    if (!games_start) {
        LOG_W("No games data for Steam ID: %s", steam_id);
        free(body);
        return 0;
    }
    pos = (size_t)(games_start - body);

    while (count < max_names &&
           json_next_value(body, &pos, "name", value, sizeof(value)))
    {
        names[count++] = _strdup(value);
    }

    free(body);
    LOG_I("Found %d game(s) for Steam ID: %s", count, steam_id);
    return count;
}

/* ------------------------------------------------------------------ */
/*  HLTB API: get main-story completion time for a game                */
/* ------------------------------------------------------------------ */
/*
 * Posts to https://howlongtobeat.com/api/search/<hash>
 *
 * HLTB uses an API key derived from hashing their JavaScript bundle.
 * This implementation uses a known search endpoint that works without
 * an API key for reasonable traffic. If the API changes, update the
 * path and POST body format below.
 *
 * Returns completion time in hours, or -1.0 if not found.
 */
static double hltb_get_main_story(const char *game_name)
{
    /* Build JSON POST body */
    char post_body[1024];
    /* Escape double quotes in game name (minimal) */
    char escaped[512];
    const char *src = game_name;
    char *dst = escaped;
    while (*src && dst - escaped < (int)sizeof(escaped) - 2) {
        if (*src == '"') { *dst++ = '\\'; }
        *dst++ = *src++;
    }
    *dst = '\0';

    _snprintf(post_body, sizeof(post_body),
        "{\"searchType\":\"games\","
        "\"searchTerms\":[\"%s\"],"
        "\"searchPage\":1,"
        "\"size\":5,"
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

    char *body = http_request(
        L"howlongtobeat.com",
        L"/api/search/all",
        1, /* HTTPS */
        post_body,
        L"Content-Type: application/json");

    if (!body) return -1.0;

    /* Parse: look for "comp_main" or "gameplayMain" */
    double hours = -1.0;
    size_t pos = 0;
    char value[64];

    if (json_next_value(body, &pos, "comp_main", value, sizeof(value))) {
        double seconds = atof(value);
        if (seconds > 0) hours = seconds / 3600.0;
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
/*  Game result record                                                  */
/* ------------------------------------------------------------------ */
typedef struct {
    char   name[512];
    double hours;       /* < 0 means no data */
} GameResult;

static GameResult g_results[4096];
static int        g_result_count = 0;

static int game_cmp(const void *a, const void *b)
{
    double ha = ((const GameResult *)a)->hours;
    double hb = ((const GameResult *)b)->hours;
    if (ha < hb) return -1;
    if (ha > hb) return  1;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Save results to file                                                */
/* ------------------------------------------------------------------ */
static void save_game_results(const char *output_file)
{
    /* Filter to only games with completion data */
    int valid_count = 0;
    for (int i = 0; i < g_result_count; i++)
        if (g_results[i].hours >= 0) valid_count++;

    if (valid_count == 0) {
        LOG_W("No games had completion data.");
        return;
    }

    /* Sort by hours */
    qsort(g_results, g_result_count, sizeof(GameResult), game_cmp);

    int max_lines = g_result_count + 8;
    char **lines = (char **)malloc(max_lines * sizeof(char *));
    char  *bufs  = (char  *)malloc(max_lines * 512);
    if (!lines || !bufs) { free(lines); free(bufs); return; }

    int lc = 0;
    double total_hours = 0;
    for (int i = 0; i < g_result_count; i++) {
        if (g_results[i].hours < 0) continue;
        lines[lc] = bufs + lc * 512;
        _snprintf(lines[lc], 512, "%s: %.1f hours",
                  g_results[i].name, g_results[i].hours);
        total_hours += g_results[i].hours;
        lc++;
    }

#define LINE(fmt, ...) do { \
    lines[lc] = bufs + lc * 512; \
    _snprintf(lines[lc], 512, fmt, ##__VA_ARGS__); \
    lc++; } while(0)

    LINE("");
    LINE("Total games with completion data: %d", valid_count);
    LINE("Total games processed: %d", g_result_count);
    LINE("Games without data: %d", g_result_count - valid_count);
    if (valid_count > 0) {
        LINE("Total completion time: %.1f hours", total_hours);
        LINE("Average completion time: %.1f hours", total_hours / valid_count);
    }
#undef LINE

    save_results_to_file((const char **)lines, lc,
                          output_file, "STEAM GAMES BY COMPLETION TIME");
    free(lines);
    free(bufs);
}

/* ------------------------------------------------------------------ */
/*  Entry point                                                         */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    log_init(LOG_INFO, NULL);
    LOG_I("Steam Game Duration Analyzer");

    /* ----------------------------------------------------------------
     * Configuration: replace with your actual values.
     * Can also be supplied as:  steamSorter.exe <API_KEY> <STEAMID> ...
     * ---------------------------------------------------------------- */
    const char *api_key = "";  /* e.g. "ABCDEF1234567890ABCDEF" */
    const char *default_ids[] = {
        "",   /* Replace with actual Steam ID */
        NULL
    };

    const char  *actual_key = api_key;
    const char **steam_ids  = default_ids;
    int          id_count   = 0;

    /* Command-line overrides */
    if (argc >= 3) {
        actual_key = argv[1];
        steam_ids  = (const char **)(argv + 2);
        id_count   = argc - 2;
    } else {
        /* Count non-empty default IDs */
        for (int i = 0; default_ids[i] != NULL; i++)
            if (default_ids[i][0] != '\0') id_count++;
    }

    if (!actual_key || actual_key[0] == '\0') {
        LOG_E("Steam API key is required.");
        LOG_E("Get one from https://steamcommunity.com/dev/apikey");
        LOG_E("Then edit this source file or pass as first argument.");
        return 1;
    }
    if (id_count == 0) {
        LOG_E("At least one Steam ID is required.");
        LOG_E("Edit the steam_ids array in this source or pass as arguments.");
        return 1;
    }

    LOG_I("Analyzing %d Steam library/libraries...", id_count);

    /* Collect all unique game names */
    char *all_names[8192];
    int   all_count = 0;

    for (int i = 0; i < id_count; i++) {
        const char *sid = steam_ids[i];
        if (!sid || sid[0] == '\0') continue;

        char *batch[2048];
        int n = steam_get_game_names(actual_key, sid, batch, 2048);
        if (n <= 0) continue;

        for (int j = 0; j < n; j++) {
            /* Deduplicate */
            int dup = 0;
            for (int k = 0; k < all_count; k++)
                if (_stricmp(all_names[k], batch[j]) == 0) {
                    dup = 1; break;
                }
            if (!dup && all_count < 8192)
                all_names[all_count++] = batch[j];
            else
                free(batch[j]);
        }
    }

    if (all_count == 0) {
        LOG_W("No games found in any of the provided libraries!");
        return 0;
    }
    LOG_I("Total unique games: %d. Looking up HLTB completion times...",
          all_count);

    /* Look up each game on HLTB */
    ProgressReporter pr;
    progress_init(&pr, all_count, "HLTB lookup");

    for (int i = 0; i < all_count; i++) {
        double h = hltb_get_main_story(all_names[i]);
        if (h >= 0) {
            LOG_I("OK  %s: %.1f hours", all_names[i], h);
        } else {
            LOG_W("N/A %s: no data", all_names[i]);
        }

        if (g_result_count < 4096) {
            _snprintf(g_results[g_result_count].name,
                      sizeof(g_results[0].name), "%s", all_names[i]);
            g_results[g_result_count].hours = h;
            g_result_count++;
        }

        free(all_names[i]);
        progress_update(&pr, 1);
    }
    progress_finish(&pr);

    /* Summary */
    int with_data = 0;
    for (int i = 0; i < g_result_count; i++)
        if (g_results[i].hours >= 0) with_data++;

    LOG_I("\nSummary:");
    LOG_I("Total games analyzed: %d", g_result_count);
    LOG_I("Games with completion data: %d", with_data);
    LOG_I("Games without data: %d", g_result_count - with_data);

    const char *output_file = "steam_games_completion_times.txt";
    save_game_results(output_file);
    LOG_I("Analysis complete! Results saved to %s", output_file);

    log_close();
    return 0;
}
