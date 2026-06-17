#ifndef __PC_MONITOR_H
#define __PC_MONITOR_H

#include "stm32f4xx.h"

/* --- Claude status enum --- */
typedef enum {
    CLAUDE_IDLE = 0,
    CLAUDE_THINKING = 1,
    CLAUDE_EXECUTING = 2,
    CLAUDE_WAITING = 3,
    CLAUDE_DONE = 4
} ClaudeStatus;

/* --- PC hardware data --- */
typedef struct {
    char     date_str[12];  /* YYYY-MM-DD from PC */
    char     time_str[12];  /* HH:MM:SS from PC clock */
    uint8_t  weekday;       /* 0=Mon .. 6=Sun */
    float    cpu_temp;      /* CPU temperature (C), -1 = unavailable */
    uint8_t  cpu_usage;     /* CPU usage 0-100 */
    float    gpu_temp;      /* GPU temperature (C), -1 = no GPU */
    uint8_t  gpu_usage;     /* GPU usage 0-100, -1 = no GPU */
    uint8_t  mem_usage;     /* Memory usage 0-100 */
    ClaudeStatus claude;    /* Claude Code state */
    /* Connection tracking */
    uint32_t last_update_ms; /* System tick of last valid JSON received */
    uint8_t  is_connected;   /* 1 = receiving data, 0 = disconnected */
    uint8_t  error_count;    /* Consecutive parse errors */
    /* DHT11 sensor */
    uint8_t  dht11_temp;     /* Room temperature (C), 0xFF = no sensor */
    uint8_t  dht11_humi;     /* Room humidity (%), 0xFF = no sensor */
    char     song[64];       /* Media title from PC */
    char     artist[48];     /* Media artist from PC */
    char     lyrics[256];    /* Lyrics text from PC */
} PcData;

/* --- Global instance --- */
extern PcData g_pc_data;

/* --- Text labels for Claude status --- */
#define CLAUDE_LABEL_IDLE      "Ready"
#define CLAUDE_LABEL_THINKING  "Thinking"
#define CLAUDE_LABEL_EXECUTING "Working"
#define CLAUDE_LABEL_WAITING   "Waiting"
#define CLAUDE_LABEL_DONE      "Done!"

/* --- Public API --- */
void PC_Monitor_Init(void);

/* Parse one JSON line from serial buffer.
 * Returns 1 if a complete JSON object was successfully parsed,
 * 0 if input was a command (not JSON),
 * -1 if JSON parse failed (malformed). */
int8_t PC_Monitor_Parse(const char *line, uint16_t len);

/* Get the display label for current Claude status */
const char * PC_Monitor_GetStatusLabel(void);

/* Check if PC data is stale (no update for > 3 seconds) */
uint8_t PC_Monitor_IsStale(void);

/* Reset connection tracking */
void PC_Monitor_ResetConnection(void);

#endif /* __PC_MONITOR_H */
