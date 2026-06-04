#include "pc_monitor.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

PcData g_pc_data;

/* --- Forward helper declarations --- */
static float parse_float_field(const char *json, const char *key);
static int   parse_int_field(const char *json, const char *key);
static void  parse_claude_field(const char *json, const char *key, ClaudeStatus *out);
static const char* find_json_value(const char *json, const char *key);

void PC_Monitor_Init(void)
{
    memset(&g_pc_data, 0, sizeof(PcData));
    g_pc_data.cpu_temp = -1.0f;
    g_pc_data.gpu_temp = -1.0f;
    g_pc_data.gpu_usage = 0xFF;  /* 0xFF means "no data" for uint8_t */
    g_pc_data.is_connected = 0;
}

/* Find value for key in JSON. Returns pointer to the value start or NULL.
 * Handles: "key":value for both string and numeric values.
 * Simple scanner — not a full JSON parser, but works for our known format. */
static const char* find_json_value(const char *json, const char *key)
{
    char search[32];
    /* Build search pattern: "key": */
    int slen = snprintf(search, sizeof(search), "\"%s\":", key);
    if (slen < 3) return NULL;

    const char *pos = strstr(json, search);
    if (!pos) return NULL;
    return pos + slen;  /* Points to the value after "key": */
}

static float parse_float_field(const char *json, const char *key)
{
    const char *val = find_json_value(json, key);
    if (!val) return -1.0f;

    /* Skip whitespace */
    while (*val == ' ') val++;

    return (float)atof(val);
}

static int parse_int_field(const char *json, const char *key)
{
    const char *val = find_json_value(json, key);
    if (!val) return -1;

    while (*val == ' ') val++;
    return atoi(val);
}

static void parse_claude_field(const char *json, const char *key, ClaudeStatus *out)
{
    const char *val = find_json_value(json, key);
    if (!val) return;

    /* Skip whitespace and opening quote */
    while (*val == ' ' || *val == '"') val++;

    if (strncmp(val, "idle", 4) == 0)
        *out = CLAUDE_IDLE;
    else if (strncmp(val, "thinking", 8) == 0)
        *out = CLAUDE_THINKING;
    else if (strncmp(val, "executing", 9) == 0)
        *out = CLAUDE_EXECUTING;
    else if (strncmp(val, "waiting", 7) == 0)
        *out = CLAUDE_WAITING;
    else if (strncmp(val, "done", 4) == 0)
        *out = CLAUDE_DONE;
    /* else: keep previous status */
}

/* Extract a quoted string value for a JSON key. Returns number of chars copied. */
static uint8_t parse_string_field(const char *json, const char *key, char *out, uint8_t max_len)
{
    const char *val = find_json_value(json, key);
    if (!val) return 0;

    /* Skip whitespace and opening quote */
    while (*val == ' ' || *val == '"') val++;

    uint8_t n = 0;
    while (*val && *val != '"' && *val != '\r' && *val != '\n' && n < max_len - 1) {
        out[n++] = *val++;
    }
    out[n] = '\0';
    return n;
}

int8_t PC_Monitor_Parse(const char *line, uint16_t len)
{
    /* Quick check: JSON lines start with '{' */
    if (len < 2 || line[0] != '{') {
        return 0;  /* Not JSON — likely a command */
    }

    /* Validate: line should end with '}' (before newline) */
    const char *end = line + len - 1;
    while (end > line && (*end == '\r' || *end == '\n' || *end == ' ')) {
        end--;
    }
    if (*end != '}') {
        g_pc_data.error_count++;
        return -1;
    }

    /* Parse time field: search for "time": then extract value */
    {
        const char *tp = strstr(line, "\"time\":");
        if (tp) {
            tp += 7;  /* skip past "time": */
            /* Skip whitespace and opening quote */
            while (*tp == ' ' || *tp == '"') tp++;
            uint8_t n = 0;
            while (*tp && *tp != '"' && n < sizeof(g_pc_data.time_str) - 1) {
                g_pc_data.time_str[n++] = *tp++;
            }
            g_pc_data.time_str[n] = '\0';
        }
    }

    float cpu_temp = parse_float_field(line, "cpu_temp");
    int cpu_usage  = parse_int_field(line, "cpu_usage");
    float gpu_temp = parse_float_field(line, "gpu_temp");
    int gpu_usage  = parse_int_field(line, "gpu_usage");
    int mem_usage  = parse_int_field(line, "mem_usage");

    /* Only accept if we got at least CPU and MEM values */
    if (cpu_usage < 0 || mem_usage < 0) {
        g_pc_data.error_count++;
        return -1;
    }

    /* Update stored data */
    g_pc_data.cpu_temp  = cpu_temp;
    g_pc_data.cpu_usage = (uint8_t)((cpu_usage > 100) ? 100 : cpu_usage);
    g_pc_data.gpu_temp  = gpu_temp;
    g_pc_data.gpu_usage = (gpu_usage < 0) ? 0xFF : (uint8_t)((gpu_usage > 100) ? 100 : gpu_usage);
    g_pc_data.mem_usage = (uint8_t)((mem_usage > 100) ? 100 : mem_usage);

    parse_claude_field(line, "claude", &g_pc_data.claude);

    /* Connection tracking */
    g_pc_data.is_connected = 1;
    g_pc_data.error_count = 0;
    /* g_pc_data.last_update_ms set by caller (main.c) */

    return 1;
}

const char * PC_Monitor_GetStatusLabel(void)
{
    switch (g_pc_data.claude) {
        case CLAUDE_IDLE:      return CLAUDE_LABEL_IDLE;
        case CLAUDE_THINKING:  return CLAUDE_LABEL_THINKING;
        case CLAUDE_EXECUTING: return CLAUDE_LABEL_EXECUTING;
        case CLAUDE_WAITING:   return CLAUDE_LABEL_WAITING;
        case CLAUDE_DONE:      return CLAUDE_LABEL_DONE;
        default:               return CLAUDE_LABEL_IDLE;
    }
}

uint8_t PC_Monitor_IsStale(void)
{
    if (!g_pc_data.is_connected) return 1;
    return 0;  /* last_update_ms checked in main loop */
}

void PC_Monitor_ResetConnection(void)
{
    g_pc_data.is_connected = 0;
    g_pc_data.error_count = 0;
}
