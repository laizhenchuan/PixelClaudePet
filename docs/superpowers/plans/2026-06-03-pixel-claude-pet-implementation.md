# Pixel Claude Pet — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a PC status monitor that sends CPU/GPU/memory data + Claude Code status via serial to STM32F407, displayed on ILI9341 LCD with a Claude pixel-art character.

**Architecture:** PC side is a Python Windows Service reading hardware stats (psutil/GPUtil) and Claude status (file-based IPC). STM32 side adds `pc_monitor.c` for JSON parsing and state management, extends `pet_render.c` with Claude character drawing and PC status overlay, and extends `pet_command.c` for JSON line routing. Dual-mode: PC monitor (default) + pet game (serial `pet` command).

**Tech Stack:** Python 3 (psutil, GPUtil, pyserial, pywin32), C (Keil MDK, STM32F4 StdPeriph), JSON lines @115200-8N1

**Spec:** [2026-06-03-pixel-claude-pet-design.md](../specs/2026-06-03-pixel-claude-pet-design.md)

**Display:** ILI9341 240×320 portrait mode. LCD_WIDTH=240, LCD_HEIGHT=320.

---

## File Structure

```
D:\Apps\pixel_pet\
├── pc_host\                         # NEW: PC-side Python program
│   ├── config.json                  # Create
│   ├── requirements.txt             # Create
│   ├── sys_monitor.py               # Create
│   ├── status_reader.py             # Create
│   ├── serial_sender.py             # Create
│   ├── pc_monitor_service.py        # Create
│   └── install_service.bat          # Create
├── app\
│   ├── pc_monitor.h                 # Create
│   ├── pc_monitor.c                 # Create
│   ├── pet_core.h                   # Modify (add is_pc_mode)
│   ├── pet_render.h                 # Modify (add PC render API)
│   ├── pet_render.c                 # Modify (Claude face + PC layout)
│   ├── pet_command.h                # Modify (add JSON routing)
│   └── pet_command.c                # Modify (JSON detect, pc/pet commands)
└── User\
    └── main.c                       # Modify (integrate pc_monitor)
```

---

## Phase 1: PC Host (Python)

### Task 1: Project scaffolding and config

**Files:**
- Create: `D:\Apps\pixel_pet\pc_host\config.json`
- Create: `D:\Apps\pixel_pet\pc_host\requirements.txt`

- [ ] **Step 1: Create config.json**

```json
{
    "serial": {
        "port": "COM3",
        "baudrate": 115200,
        "timeout": 1
    },
    "monitor": {
        "interval_sec": 1.0
    },
    "claude": {
        "status_file": "C:\\Users\\29918\\.claude\\claude_status.txt",
        "default_status": "idle"
    },
    "service": {
        "log_file": "C:\\Users\\29918\\pixel_claude_pet.log"
    }
}
```

- [ ] **Step 2: Create requirements.txt**

```
psutil>=5.9.0
GPUtil>=1.4.0
pyserial>=3.5
pywin32>=306
```

- [ ] **Step 3: Verify deps install**

Run: `pip install -r D:\Apps\pixel_pet\pc_host\requirements.txt`
Expected: All packages install without error.

---

### Task 2: sys_monitor.py — Hardware monitoring

**Files:**
- Create: `D:\Apps\pixel_pet\pc_host\sys_monitor.py`

- [ ] **Step 1: Write sys_monitor.py**

```python
"""System hardware monitor — reads CPU/GPU/Memory stats."""
import psutil

try:
    import GPUtil
    HAS_GPU = True
except ImportError:
    HAS_GPU = False


def get_cpu_temp() -> float:
    """Get CPU package temperature in Celsius. Returns -1 if unavailable."""
    try:
        temps = psutil.sensors_temperatures()
        if 'coretemp' in temps:
            for entry in temps['coretemp']:
                if 'package' in entry.label.lower():
                    return entry.current
            # Fallback: average of all core temps
            vals = [e.current for e in temps['coretemp']]
            if vals:
                return sum(vals) / len(vals)
        # Try other common sensor names
        for key in temps:
            vals = [e.current for e in temps[key]]
            if vals:
                return sum(vals) / len(vals)
    except Exception:
        pass
    return -1.0


def get_cpu_usage() -> int:
    """Get CPU utilization percentage (0-100)."""
    return int(psutil.cpu_percent(interval=0.1))


def get_gpu_temp() -> float:
    """Get GPU temperature in Celsius. Returns -1 if no GPU or unavailable."""
    if not HAS_GPU:
        return -1.0
    try:
        gpus = GPUtil.getGPUs()
        if gpus:
            return gpus[0].temperature
    except Exception:
        pass
    return -1.0


def get_gpu_usage() -> int:
    """Get GPU utilization percentage (0-100). Returns -1 if no GPU."""
    if not HAS_GPU:
        return -1
    try:
        gpus = GPUtil.getGPUs()
        if gpus:
            return int(gpus[0].load * 100)
    except Exception:
        pass
    return -1


def get_mem_usage() -> int:
    """Get physical memory usage percentage (0-100)."""
    return int(psutil.virtual_memory().percent)


def collect_all() -> dict:
    """Collect all hardware stats into a dict."""
    return {
        'cpu_temp': get_cpu_temp(),
        'cpu_usage': get_cpu_usage(),
        'gpu_temp': get_gpu_temp(),
        'gpu_usage': get_gpu_usage(),
        'mem_usage': get_mem_usage(),
    }
```

- [ ] **Step 2: Test sys_monitor standalone**

Run: `python -c "import sys; sys.path.insert(0, r'D:\Apps\pixel_pet\pc_host'); from sys_monitor import collect_all; print(collect_all())"`
Expected: Prints dict with cpu_temp, cpu_usage, gpu_temp, gpu_usage, mem_usage values.

---

### Task 3: status_reader.py — Claude status file reader

**Files:**
- Create: `D:\Apps\pixel_pet\pc_host\status_reader.py`

- [ ] **Step 1: Write status_reader.py**

```python
"""Reads Claude Code status from a shared file.

Claude Code hooks write the current status to this file.
If the file doesn't exist or can't be read, returns default status.
"""
import os

VALID_STATUSES = {'idle', 'thinking', 'executing', 'waiting', 'done'}


class StatusReader:
    def __init__(self, status_file: str, default: str = 'idle'):
        self._file = status_file
        self._default = default
        self._last_status = default

    def read(self) -> str:
        """Read current Claude status. Falls back to default on error."""
        try:
            if not os.path.exists(self._file):
                self._last_status = self._default
                return self._default
            with open(self._file, 'r', encoding='utf-8') as f:
                raw = f.read().strip()
            if raw in VALID_STATUSES:
                self._last_status = raw
                return raw
            # Unknown value — keep last known status
            return self._last_status
        except Exception:
            return self._last_status

    def get_last(self) -> str:
        return self._last_status
```

- [ ] **Step 2: Quick test**

Run: `python -c "import sys; sys.path.insert(0, r'D:\Apps\pixel_pet\pc_host'); from status_reader import StatusReader; sr = StatusReader('/nonexistent/file.txt'); print(sr.read())"`
Expected: Prints `idle`

---

### Task 4: serial_sender.py — Serial communication

**Files:**
- Create: `D:\Apps\pixel_pet\pc_host\serial_sender.py`

- [ ] **Step 1: Write serial_sender.py**

```python
"""Serial sender — manages COM port connection and JSON line sending."""
import json
import logging
import time

import serial

log = logging.getLogger(__name__)


class SerialSender:
    def __init__(self, port: str, baudrate: int = 115200, timeout: float = 1.0):
        self._port = port
        self._baudrate = baudrate
        self._timeout = timeout
        self._ser = None

    def connect(self) -> bool:
        """Open serial port. Returns True on success."""
        try:
            self._ser = serial.Serial(
                port=self._port,
                baudrate=self._baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=self._timeout,
            )
            log.info("Connected to %s @ %d baud", self._port, self._baudrate)
            return True
        except serial.SerialException as e:
            log.error("Serial open failed (%s): %s", self._port, e)
            self._ser = None
            return False

    def send(self, data: dict) -> bool:
        """Send one JSON line. Returns True on success."""
        if self._ser is None or not self._ser.is_open:
            return False
        try:
            line = json.dumps(data, ensure_ascii=False) + '\n'
            self._ser.write(line.encode('utf-8'))
            return True
        except (serial.SerialException, OSError) as e:
            log.error("Send failed: %s", e)
            self._ser = None
            return False

    def close(self):
        if self._ser and self._ser.is_open:
            self._ser.close()
        self._ser = None

    @property
    def is_connected(self) -> bool:
        return self._ser is not None and self._ser.is_open
```

- [ ] **Step 2: Import check**

Run: `python -c "import sys; sys.path.insert(0, r'D:\Apps\pixel_pet\pc_host'); from serial_sender import SerialSender; print('OK')"`
Expected: Prints `OK`

---

### Task 5: pc_monitor_service.py — Windows Service main program

**Files:**
- Create: `D:\Apps\pixel_pet\pc_host\pc_monitor_service.py`

- [ ] **Step 1: Write pc_monitor_service.py**

```python
"""Pixel Claude Pet — Windows Service for PC status monitoring.

Sends system hardware stats + Claude Code state to STM32 via serial.
Install: python pc_monitor_service.py install
Start:   python pc_monitor_service.py start
Stop:    python pc_monitor_service.py stop
Remove:  python pc_monitor_service.py remove
Debug:   python pc_monitor_service.py debug  (run in foreground)
"""
import json
import logging
import os
import sys
import time
from pathlib import Path

import servicemanager
import win32event
import win32service
import win32serviceutil

# Add script dir to path for sibling imports
SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))

from sys_monitor import collect_all
from status_reader import StatusReader
from serial_sender import SerialSender

CONFIG_PATH = SCRIPT_DIR / 'config.json'


def load_config():
    with open(CONFIG_PATH, 'r', encoding='utf-8') as f:
        return json.load(f)


class PcMonitorService(win32serviceutil.ServiceFramework):
    _svc_name_ = 'PixelClaudePet'
    _svc_display_name_ = 'Pixel Claude Pet — PC Monitor Service'
    _svc_description_ = 'Sends PC hardware stats and Claude Code status to STM32 via serial port.'

    def __init__(self, args):
        win32serviceutil.ServiceFramework.__init__(self, args)
        self._stop_event = win32event.CreateEvent(None, 0, 0, None)
        self._sender = None
        self._status_reader = None
        self._config = None

    def SvcStop(self):
        self.ReportServiceStatus(win32service.SERVICE_STOP_PENDING)
        win32event.SetEvent(self._stop_event)
        if self._sender:
            self._sender.close()

    def SvcDoRun(self):
        servicemanager.LogMsg(
            servicemanager.EVENTLOG_INFORMATION_TYPE,
            servicemanager.PYS_SERVICE_STARTED,
            (self._svc_name_, '')
        )
        self._run()

    def _run(self):
        self._config = load_config()

        # Setup logging
        log_file = self._config.get('service', {}).get('log_file', 'pixel_claude_pet.log')
        logging.basicConfig(
            filename=log_file,
            level=logging.INFO,
            format='%(asctime)s [%(levelname)s] %(message)s',
        )

        # Init modules
        status_file = self._config['claude']['status_file']
        default_status = self._config['claude']['default_status']
        self._status_reader = StatusReader(status_file, default_status)

        port = self._config['serial']['port']
        baudrate = self._config['serial']['baudrate']
        timeout = self._config['serial'].get('timeout', 1.0)
        self._sender = SerialSender(port, baudrate, timeout)

        interval = self._config['monitor']['interval_sec']

        logging.info("Service started. Port=%s, interval=%.1fs", port, interval)

        while True:
            # Check stop signal
            if win32event.WaitForSingleObject(self._stop_event, 0) == win32event.WAIT_OBJECT_0:
                break

            # Collect data
            data = collect_all()
            data['claude'] = self._status_reader.read()

            # Ensure connection and send
            if not self._sender.is_connected:
                logging.info("Attempting serial connect to %s...", port)
                if not self._sender.connect():
                    logging.warning("Connect failed, retry in %ds", int(interval * 5))
                    time.sleep(interval * 5)
                    continue

            if self._sender.send(data):
                logging.debug("Sent: %s", json.dumps(data))
            else:
                logging.warning("Send failed, will reconnect next cycle")

            # Sleep until next interval or stop
            win32event.WaitForSingleObject(self._stop_event, int(interval * 1000))


# Allow running as regular script for debugging
if __name__ == '__main__':
    if len(sys.argv) == 1:
        # Default: run in debug/foreground mode
        print("Running in debug mode (Ctrl+C to stop)...")
        print("Install as service: python pc_monitor_service.py install")
        print("Start service:      python pc_monitor_service.py start")
        print("")
        config = load_config()
        sender = SerialSender(
            config['serial']['port'],
            config['serial']['baudrate'],
            config['serial'].get('timeout', 1.0),
        )
        status_reader = StatusReader(
            config['claude']['status_file'],
            config['claude']['default_status'],
        )
        interval = config['monitor']['interval_sec']

        try:
            while True:
                data = collect_all()
                data['claude'] = status_reader.read()
                data_str = json.dumps(data, ensure_ascii=False)
                print(f"\r{data_str}", end='', flush=True)
                if sender.is_connected:
                    sender.send(data)
                elif not sender.connect():
                    print(f"\n[!] Waiting for {config['serial']['port']}...")
                    time.sleep(5)
                    continue
                time.sleep(interval)
        except KeyboardInterrupt:
            print("\n\nStopped.")
            sender.close()
    else:
        win32serviceutil.HandleCommandLine(PcMonitorService)
```

- [ ] **Step 2: Syntax check**

Run: `python -m py_compile D:\Apps\pixel_pet\pc_host\pc_monitor_service.py`
Expected: No output (compiles OK).

---

### Task 6: install_service.bat — Installation script

**Files:**
- Create: `D:\Apps\pixel_pet\pc_host\install_service.bat`

- [ ] **Step 1: Write install_service.bat**

```bat
@echo off
chcp 65001 >nul
title Pixel Claude Pet - Service Manager

echo ========================================
echo   Pixel Claude Pet - Service Manager
echo ========================================
echo.
echo Choose an option:
echo   1. Install service
echo   2. Start service
echo   3. Stop service
echo   4. Remove service
echo   5. Debug run (foreground)
echo.

set /p choice="Enter choice (1-5): "

cd /d "%~dp0"

if "%choice%"=="1" (
    echo Installing service...
    python pc_monitor_service.py install
    python pc_monitor_service.py start
    echo Done! Service installed and started.
)
if "%choice%"=="2" (
    echo Starting service...
    python pc_monitor_service.py start
)
if "%choice%"=="3" (
    echo Stopping service...
    python pc_monitor_service.py stop
)
if "%choice%"=="4" (
    echo Removing service...
    python pc_monitor_service.py stop
    python pc_monitor_service.py remove
    echo Done! Service removed.
)
if "%choice%"=="5" (
    echo Running in debug mode...
    python pc_monitor_service.py
)
pause
```

- [ ] **Step 2: Verify batch file syntax**

Run: `cmd /c "D:\Apps\pixel_pet\pc_host\install_service.bat" < nul` (will show menu, can Ctrl+C)
Expected: Menu displays without errors.

---

## Phase 2: STM32 Firmware (C)

### Task 7: pc_monitor.h — PC data structures and API

**Files:**
- Create: `D:\Apps\pixel_pet\app\pc_monitor.h`

- [ ] **Step 1: Write pc_monitor.h**

```c
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
} PcData;

/* --- Global instance --- */
extern PcData g_pc_data;

/* --- Text labels for Claude status (English, fits LCD) --- */
#define CLAUDE_LABEL_IDLE      "Ready"
#define CLAUDE_LABEL_THINKING  "Thinking..."
#define CLAUDE_LABEL_EXECUTING "Working..."
#define CLAUDE_LABEL_WAITING   "Waiting..."
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
```

---

### Task 8: pc_monitor.c — JSON parser and state management

**Files:**
- Create: `D:\Apps\pixel_pet\app\pc_monitor.c`

- [ ] **Step 1: Write pc_monitor.c**

```c
#include "pc_monitor.h"
#include <string.h>
#include <stdio.h>

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

    /* Parse all known fields */
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
```

- [ ] **Step 2: Syntax check (compile test)**

Check that the file has no obvious C syntax errors by reviewing. Then add to Keil project in later steps.

---

### Task 9: Modify pet_core.h — Add PC mode flag

**Files:**
- Modify: `D:\Apps\pixel_pet\app\pet_core.h`

- [ ] **Step 1: Add is_pc_mode field to PetData struct**

Open [pet_core.h](D:\Apps\pixel_pet\app\pet_core.h) and add after line 37 (`uint16_t sick_duration;`):

```c
    uint16_t sick_duration;
    uint8_t  is_pc_mode;    /* 1 = PC monitor mode, 0 = pet game mode */
```

- [ ] **Step 2: Initialize is_pc_mode in Pet_Init()**

Open [pet_core.c](D:\Apps\pixel_pet\app\pet_core.c), find `Pet_Init()`, add after `g_pet.current_mood = MOOD_HAPPY;`:

```c
    g_pet.current_mood = MOOD_HAPPY;
    g_pet.is_pc_mode = 1;  /* Start in PC monitor mode by default */
```

---

### Task 10: Modify pet_render.h — Add PC mode rendering API

**Files:**
- Modify: `D:\Apps\pixel_pet\app\pet_render.h`

- [ ] **Step 1: Add PC mode layout constants and API declarations**

After the existing `#define STAT_RIGHT_X 128` line, add:

```c
/* ---- PC Monitor Mode Layout ---- */
/* Screen is 240x320 portrait; LCD_WIDTH=240, LCD_HEIGHT=320 */

/* PC status bar area (top section) */
#define PC_STATUS_Y         5
#define PC_STATUS_LINE_H    18   /* Height per status line */
#define PC_STATUS_BAR_W     100  /* Width of progress bar */
#define PC_STATUS_BAR_X     130  /* X start of progress bar */
#define PC_STATUS_BAR_H     8    /* Height of progress bar */

/* Claude character area (middle section) */
#define CLAUDE_AREA_Y       75
#define CLAUDE_AREA_H       155   /* Height for Claude face + status text */
#define CLAUDE_CX           120  /* Center X of Claude face */
#define CLAUDE_CY           150  /* Center Y of Claude face */

/* Status text area */
#define STATUS_TEXT_Y       230

/* Bottom info bar */
#define PC_INFO_BAR_Y       280
#define PC_INFO_BAR_H       40

/* Claude character colors (orange/warm palette) */
#define CLAUDE_BODY_COLOR   0xFD08  /* Warm orange */
#define CLAUDE_LIGHT_COLOR  0xFE95  /* Light orange highlight */
#define CLAUDE_DARK_COLOR   0xEAA4  /* Darker orange shade */
#define CLAUDE_OUTLINE      0x4228  /* Dark brown outline */
#define CLAUDE_EYE_WHITE    0xFFFF  /* White */
#define CLAUDE_EYE_PUPIL    0x0000  /* Black pupil */
#define CLAUDE_MOUTH_COLOR  0x4228  /* Dark brown mouth */

/* ---- PC Mode Render API ---- */
void Render_PCMode(void);              /* Full PC mode screen render */
void Render_PCStatusBar(void);         /* Top status bars (CPU/GPU/MEM) */
void Render_ClaudeCharacter(uint16_t cx, uint16_t cy, uint8_t frame, ClaudeStatus mood);  /* Claude face */
void Render_StatusText(const char *text, uint16_t color);  /* Bottom status text */
```

- [ ] **Step 2: Add ClaudeStatus forward declaration at top of pet_render.h**

Since `ClaudeStatus` is defined in `pc_monitor.h`, add at the top of `pet_render.h`:

```c
#include "pc_monitor.h"  /* For ClaudeStatus enum */
```

---

### Task 11: Modify pet_render.c — Claude character + PC mode rendering

**Files:**
- Modify: `D:\Apps\pixel_pet\app\pet_render.c`

- [ ] **Step 1: Add color utility functions**

Add after the `Darken` function (before `DrawBar`):

```c
/* Blend two colors by ratio (0-255, where 0 = all c1, 255 = all c2) */
static uint16_t Blend(uint16_t c1, uint16_t c2, uint8_t ratio)
{
    uint8_t r = (((c1 >> 11) * (255 - ratio) + (c2 >> 11) * ratio) / 255);
    uint8_t g = ((((c1 >> 5) & 0x3F) * (255 - ratio) + ((c2 >> 5) & 0x3F) * ratio) / 255);
    uint8_t b = (((c1 & 0x1F) * (255 - ratio) + (c2 & 0x1F) * ratio) / 255);
    return ((uint16_t)r << 11) | ((uint16_t)g << 5) | b;
}
```

- [ ] **Step 2: Add Render_ClaudeCharacter function**

Add at end of file (before `Render_Init`):

```c
/* ================================================================
 * CLAUDE CHARACTER — friendly AI face, orange/warm palette
 * 96x96 pixel art, drawn centered at (cx, cy)
 * frame: animation tick, mood: ClaudeStatus for expression
 * ================================================================ */
void Render_ClaudeCharacter(uint16_t cx, uint16_t cy, uint8_t frame, ClaudeStatus mood)
{
    /* Head dimensions */
    uint16_t head_w = 48;  /* half-width of head */
    uint16_t head_h = 42;  /* half-height of head */

    uint16_t body  = CLAUDE_BODY_COLOR;
    uint16_t light = CLAUDE_LIGHT_COLOR;
    uint16_t dark  = CLAUDE_DARK_COLOR;
    uint16_t outl  = CLAUDE_OUTLINE;

    /* Animation: gentle float (slow sine-like bob) */
    int8_t bob = 0;
    uint8_t f4 = frame % 60;
    if (f4 < 15) bob = f4 / 5;        /* 0,1,2 up */
    else if (f4 < 30) bob = 3 - (f4 - 15) / 5;  /* down */
    else if (f4 < 45) bob = -(f4 - 30) / 5;
    else bob = -3 + (f4 - 45) / 5;

    uint16_t head_y = cy + bob;

    /* --- Head body (rounded rectangle / oval) --- */
    /* Main fill */
    LCD_Fill(cx - head_w + 6, head_y - head_h + 4,
             cx + head_w - 6, head_y + head_h - 4, body);
    /* Top rounding */
    LCD_Fill(cx - head_w + 10, head_y - head_h, cx + head_w - 10, head_y - head_h + 6, body);
    LCD_Fill(cx - head_w + 6, head_y - head_h + 2, cx + head_w - 6, head_y - head_h + 4, body);
    LCD_Circle(cx - head_w + 14, head_y - head_h + 10, 10, body);
    LCD_Circle(cx + head_w - 14, head_y - head_h + 10, 10, body);
    /* Bottom rounding */
    LCD_Fill(cx - head_w + 10, head_y + head_h - 6, cx + head_w - 10, head_y + head_h, body);
    LCD_Fill(cx - head_w + 6, head_y + head_h - 4, cx + head_w - 6, head_y + head_h - 2, body);
    LCD_Circle(cx - head_w + 14, head_y + head_h - 10, 10, body);
    LCD_Circle(cx + head_w - 14, head_y + head_h - 10, 10, body);

    /* Head outline */
    LCD_Circle(cx - head_w + 14, head_y - head_h + 10, 10, outl);
    LCD_Circle(cx + head_w - 14, head_y - head_h + 10, 10, outl);
    LCD_Circle(cx - head_w + 14, head_y + head_h - 10, 10, outl);
    LCD_Circle(cx + head_w - 14, head_y + head_h - 10, 10, outl);

    /* Highlight — lighter patch on top-left */
    LCD_Fill(cx - head_w/2, head_y - head_h + 8, cx + head_w/4, head_y - 5, light);
    LCD_Circle(cx - head_w/4, head_y - head_h/3, head_w/3, light);

    /* --- Eyes (large, friendly) --- */
    uint16_t eye_y = head_y - 6;
    uint16_t eye_sp = 16;  /* eye spacing from center */
    uint16_t eye_r = 10;   /* eye radius */
    uint16_t lx = cx - eye_sp;
    uint16_t rx = cx + eye_sp;

    /* Blink animation */
    uint8_t blink = (frame % 20 >= 17);

    if (blink) {
        /* Eyes closed = horizontal lines */
        LCD_Line(lx - eye_r, eye_y, lx + eye_r, eye_y, outl);
        LCD_Line(rx - eye_r, eye_y, rx + eye_r, eye_y, outl);
    } else {
        /* Draw white of eyes */
        LCD_Fill(lx - eye_r, eye_y - eye_r, lx + eye_r, eye_y + eye_r, CLAUDE_EYE_WHITE);
        LCD_Fill(rx - eye_r, eye_y - eye_r, rx + eye_r, eye_y + eye_r, CLAUDE_EYE_WHITE);
        LCD_Circle(lx, eye_y, eye_r, outl);
        LCD_Circle(rx, eye_y, eye_r, outl);

        /* Pupils — different based on mood */
        uint16_t pupil_r = 4;
        uint8_t pupil_shift = 0;

        if (mood == CLAUDE_THINKING) {
            /* Eyes looking up-left (thinking hard) */
            LCD_Fill(lx - 3, eye_y - 5, lx + 3, eye_y + 1, CLAUDE_EYE_PUPIL);
            LCD_Fill(rx - 3, eye_y - 5, rx + 3, eye_y + 1, CLAUDE_EYE_PUPIL);
            /* Furrowed brow */
            LCD_Line(lx - eye_r, eye_y - eye_r - 2, lx + eye_r - 2, eye_y - eye_r + 1, outl);
            LCD_Line(rx - eye_r + 2, eye_y - eye_r + 1, rx + eye_r, eye_y - eye_r - 2, outl);
        } else if (mood == CLAUDE_DONE) {
            /* Happy ^_^ eyes (arcs) */
            LCD_Line(lx - eye_r, eye_y - 3, lx, eye_y - eye_r + 2, outl);
            LCD_Line(lx, eye_y - eye_r + 2, lx + eye_r, eye_y - 3, outl);
            LCD_Line(rx - eye_r, eye_y - 3, rx, eye_y - eye_r + 2, outl);
            LCD_Line(rx, eye_y - eye_r + 2, rx + eye_r, eye_y - 3, outl);
            /* Small pupil dots */
            LCD_Fill(lx - 1, eye_y, lx + 1, eye_y + 2, CLAUDE_EYE_PUPIL);
            LCD_Fill(rx - 1, eye_y, rx + 1, eye_y + 2, CLAUDE_EYE_PUPIL);
        } else if (mood == CLAUDE_WAITING) {
            /* Slightly tilted — one eye bigger */
            LCD_Fill(lx - 3, eye_y - 3, lx + 3, eye_y + 3, CLAUDE_EYE_PUPIL);
            LCD_Fill(rx - 1, eye_y - 2, rx + 2, eye_y + 3, CLAUDE_EYE_PUPIL);
        } else {
            /* Normal: round pupils centered */
            LCD_Fill(lx - pupil_r, eye_y - pupil_r, lx + pupil_r, eye_y + pupil_r, CLAUDE_EYE_PUPIL);
            LCD_Fill(rx - pupil_r, eye_y - pupil_r, rx + pupil_r, eye_y + pupil_r, CLAUDE_EYE_PUPIL);
        }

        /* Eye shine dots */
        if (!blink && mood != CLAUDE_DONE) {
            LCD_DrawPoint(lx - 4, eye_y - 6, CLAUDE_EYE_WHITE);
            LCD_DrawPoint(lx - 3, eye_y - 7, CLAUDE_EYE_WHITE);
            LCD_DrawPoint(rx - 4, eye_y - 6, CLAUDE_EYE_WHITE);
            LCD_DrawPoint(rx - 3, eye_y - 7, CLAUDE_EYE_WHITE);
        }
    }

    /* --- Mouth --- */
    uint16_t mouth_y = head_y + 14;
    uint16_t mw = 10;

    if (mood == CLAUDE_DONE) {
        /* Big happy smile */
        LCD_Line(cx - mw, mouth_y - 2, cx, mouth_y + 4, outl);
        LCD_Line(cx, mouth_y + 4, cx + mw, mouth_y - 2, outl);
        LCD_Fill(cx - mw/2, mouth_y - 1, cx + mw/2, mouth_y + 3, CLAUDE_MOUTH_COLOR);
    } else if (mood == CLAUDE_THINKING) {
        /* Slightly pursed / thoughtful */
        LCD_Line(cx - mw/2, mouth_y, cx + mw/2, mouth_y, outl);
    } else if (mood == CLAUDE_WAITING) {
        /* Slight open mouth "hmm?" */
        LCD_Line(cx - mw, mouth_y, cx + mw, mouth_y, outl);
        LCD_Fill(cx - 3, mouth_y + 1, cx + 3, mouth_y + 4, CLAUDE_MOUTH_COLOR);
    } else {
        /* Gentle smile */
        LCD_Line(cx - mw, mouth_y, cx, mouth_y + 3, outl);
        LCD_Line(cx, mouth_y + 3, cx + mw, mouth_y, outl);
    }

    /* --- Cheek blush circles --- */
    if (mood != CLAUDE_THINKING) {
        LCD_Fill(lx - 16, mouth_y - 2, lx - 10, mouth_y + 4,
                 Blend(body, RED, 40));
        LCD_Fill(rx + 10, mouth_y - 2, rx + 16, mouth_y + 4,
                 Blend(body, RED, 40));
    }

    /* --- Status-specific effects --- */
    if (mood == CLAUDE_THINKING) {
        /* Thought bubble dots */
        uint16_t dot_cy = head_y - head_h - 5 + bob;
        LCD_DrawPoint(cx + head_w - 5, dot_cy - 16, LIGHTBLUE);
        LCD_Fill(cx + head_w - 2, dot_cy - 22, cx + head_w + 2, dot_cy - 18, LIGHTBLUE);
        LCD_Fill(cx + head_w + 1, dot_cy - 30, cx + head_w + 7, dot_cy - 22, LIGHTBLUE);
    }
    if (mood == CLAUDE_DONE) {
        /* Small sparkle stars */
        if (frame % 16 < 8) {
            uint16_t sx = cx + head_w + 5 + (frame % 3) * 8;
            uint16_t sy = head_y - head_h + (frame % 4) * 6;
            LCD_Line(sx - 2, sy, sx + 2, sy, YELLOW);
            LCD_Line(sx, sy - 2, sx, sy + 2, YELLOW);
        }
    }
}
```

- [ ] **Step 2: Add Render_PCStatusBar function**

Add after `Render_ClaudeCharacter`:

```c
void Render_PCStatusBar(void)
{
    char buf[32];
    uint16_t y = PC_STATUS_Y;
    uint16_t bar_x = PC_STATUS_BAR_X;
    uint16_t bar_w = PC_STATUS_BAR_W;
    uint16_t bar_h = PC_STATUS_BAR_H;
    uint16_t label_x = 5;

    /* ---- CPU Line ---- */
    if (g_pc_data.cpu_temp > 0) {
        snprintf(buf, sizeof(buf), "CPU %.0fC", g_pc_data.cpu_temp);
    } else {
        snprintf(buf, sizeof(buf), "CPU --");
    }
    LCD_String(label_x, y, buf, 12, WHITE, BLACK);
    snprintf(buf, sizeof(buf), "%d%%", g_pc_data.cpu_usage);
    LCD_String(bar_x - 32, y, buf, 12, GREEN, BLACK);

    /* CPU bar — color shifts: green → yellow → red */
    uint16_t cpu_color = GREEN;
    if (g_pc_data.cpu_usage > 75) cpu_color = RED;
    else if (g_pc_data.cpu_usage > 50) cpu_color = YELLOW;
    DrawBar(bar_x, y + 2, bar_w, bar_h, g_pc_data.cpu_usage, cpu_color, DARKBLUE);

    /* ---- GPU Line ---- */
    y += PC_STATUS_LINE_H;
    if (g_pc_data.gpu_temp > 0) {
        snprintf(buf, sizeof(buf), "GPU %.0fC", g_pc_data.gpu_temp);
    } else if (g_pc_data.gpu_usage == 0xFF) {
        snprintf(buf, sizeof(buf), "GPU --");
    } else {
        snprintf(buf, sizeof(buf), "GPU n/a");  /* has usage but no temp sensor */
    }
    LCD_String(label_x, y, buf, 12, WHITE, BLACK);

    if (g_pc_data.gpu_usage != 0xFF) {
        snprintf(buf, sizeof(buf), "%d%%", g_pc_data.gpu_usage);
        uint16_t gpu_color = GREEN;
        if (g_pc_data.gpu_usage > 75) gpu_color = RED;
        else if (g_pc_data.gpu_usage > 50) gpu_color = YELLOW;
        LCD_String(bar_x - 32, y, buf, 12, gpu_color, BLACK);
        DrawBar(bar_x, y + 2, bar_w, bar_h, g_pc_data.gpu_usage, gpu_color, DARKBLUE);
    } else {
        LCD_String(bar_x - 32, y, "--", 12, LGRAY, BLACK);
    }

    /* ---- MEM Line ---- */
    y += PC_STATUS_LINE_H;
    snprintf(buf, sizeof(buf), "MEM");
    LCD_String(label_x, y, buf, 12, WHITE, BLACK);
    snprintf(buf, sizeof(buf), "%d%%", g_pc_data.mem_usage);
    LCD_String(bar_x - 32, y, buf, 12, CYAN, BLACK);
    DrawBar(bar_x, y + 2, bar_w, bar_h, g_pc_data.mem_usage, CYAN, DARKBLUE);

    /* ---- Separator line ---- */
    y += PC_STATUS_LINE_H + 4;
    LCD_Line(0, y, LCD_WIDTH - 1, y, DARKBLUE);
}

void Render_StatusText(const char *text, uint16_t color)
{
    /* Center the text horizontally */
    uint16_t text_len = strlen(text);
    uint16_t text_px = text_len * 8;  /* approx px width for 16-size font */
    uint16_t x = (LCD_WIDTH - text_px) / 2;
    if (x > LCD_WIDTH) x = 5;

    /* Clear area */
    LCD_Fill(0, STATUS_TEXT_Y, LCD_WIDTH - 1, STATUS_TEXT_Y + 22, BLACK);

    /* Pulse effect for active states */
    if (color == LIGHTBLUE || color == GREEN || color == YELLOW) {
        extern volatile uint32_t g_sys_tick_ms;
        if ((g_sys_tick_ms / 500) % 2 == 0) {
            LCD_String(x, STATUS_TEXT_Y, (char*)text, 16, color, BLACK);
        }
    } else {
        LCD_String(x, STATUS_TEXT_Y, (char*)text, 16, color, BLACK);
    }
}

void Render_PCMode(void)
{
    char buf[24];

    /* Top status bars */
    Render_PCStatusBar();

    /* Claude character */
    extern volatile uint32_t g_sys_tick_ms;
    uint8_t frame = (uint8_t)((g_sys_tick_ms / 100) % 256);
    Render_ClaudeCharacter(CLAUDE_CX, CLAUDE_CY, frame, g_pc_data.claude);

    /* Status text with color based on state */
    const char *label = PC_Monitor_GetStatusLabel();
    uint16_t label_color = WHITE;
    switch (g_pc_data.claude) {
        case CLAUDE_THINKING:  label_color = LIGHTBLUE; break;
        case CLAUDE_EXECUTING: label_color = GREEN;     break;
        case CLAUDE_WAITING:   label_color = YELLOW;    break;
        case CLAUDE_DONE:      label_color = GREEN;     break;
        default:               label_color = WHITE;     break;
    }
    Render_StatusText(label, label_color);

    /* Bottom info bar */
    LCD_Fill(0, PC_INFO_BAR_Y, LCD_WIDTH - 1, LCD_HEIGHT - 1, DARKBLUE);
    LCD_Line(0, PC_INFO_BAR_Y, LCD_WIDTH - 1, PC_INFO_BAR_Y, LIGHTBLUE);

    /* Connection indicator */
    if (g_pc_data.is_connected && g_pc_data.error_count == 0) {
        LCD_String(5, PC_INFO_BAR_Y + 5, "SERIAL OK", 12, GREEN, DARKBLUE);
    } else if (g_pc_data.error_count >= 5) {
        LCD_String(5, PC_INFO_BAR_Y + 5, "DATA ERR", 12, RED, DARKBLUE);
    } else {
        LCD_String(5, PC_INFO_BAR_Y + 5, "NO SIGNAL", 12, YELLOW, DARKBLUE);
    }

    /* Mode indicator */
    snprintf(buf, sizeof(buf), "%s", g_pet.name);
    LCD_String(120, PC_INFO_BAR_Y + 5, buf, 12, YELLOW, DARKBLUE);
    LCD_String(5, PC_INFO_BAR_Y + 22, "PC Mode", 12, CYAN, DARKBLUE);
}
```

- [ ] **Step 3: Modify Render_DrawAll to support PC mode**

Replace the `Render_DrawAll` function body. The current implementation at the end of [pet_render.c](D:\Apps\pixel_pet\app\pet_render.c) is:

```c
void Render_DrawAll(void)
{
    Render_UpdateStatusBar();
    Render_DrawPet(g_anim_tick);
    Render_DrawInfoBar();
    g_anim_tick++;
    if (g_anim_tick >= 200) g_anim_tick = 0;
}
```

Replace with:

```c
void Render_DrawAll(void)
{
    if (g_pet.is_pc_mode) {
        Render_PCMode();
    } else {
        Render_UpdateStatusBar();
        Render_DrawPet(g_anim_tick);
        Render_DrawInfoBar();
    }
    g_anim_tick++;
    if (g_anim_tick >= 200) g_anim_tick = 0;
}
```

---

### Task 12: Modify pet_command.c — JSON routing + pc/pet commands

**Files:**
- Modify: `D:\Apps\pixel_pet\app\pet_command.c`
- Modify: `D:\Apps\pixel_pet\app\pet_command.h`

- [ ] **Step 1: Add pc_monitor.h include to pet_command.c**

Add after existing includes at top of [pet_command.c](D:\Apps\pixel_pet\app\pet_command.c):

```c
#include "pc_monitor.h"
```

- [ ] **Step 2: Add `pc` and `pet` commands to Cmd_Execute**

In `Cmd_Execute()`, add **before** the "Unknown command" line (before `UART1_SendString("[?] Unknown. Type 'help'\r\n");`):

```c
    if (strcmp(cmd, "pc") == 0) {
        g_pet.is_pc_mode = 1;
        UART1_SendString("[OK] Switched to PC Monitor mode\r\n");
        return;
    }

    if (strcmp(cmd, "pet") == 0) {
        g_pet.is_pc_mode = 0;
        UART1_SendString("[OK] Switched to Pet mode\r\n");
        return;
    }
```

And update the help text in `Cmd_Execute` to include these commands. In the help block, add after the `save` line:

```c
        UART1_SendString(" pc      - Switch to PC monitor mode\r\n");
        UART1_SendString(" pet     - Switch to Pet game mode\r\n");
```

- [ ] **Step 3: Route JSON lines in Cmd_Process**

In `Cmd_Process()`, right before `Cmd_Execute(g_cmd_buf)`, add JSON routing:

Find the lines:
```c
                UART1_SendString("\r\n");
                Cmd_Execute(g_cmd_buf);
```

Replace with:

```c
                /* Check if this is a JSON line from PC monitor */
                if (g_cmd_buf[0] == '{') {
                    /* Don't echo JSON — it's machine-to-machine */
                    int8_t result = PC_Monitor_Parse(g_cmd_buf, g_cmd_len);
                    if (result == 1) {
                        /* Success — update timestamp */
                        extern volatile uint32_t g_sys_tick_ms;
                        g_pc_data.last_update_ms = g_sys_tick_ms;
                    }
                } else {
                    UART1_SendString("\r\n");
                    Cmd_Execute(g_cmd_buf);
                }
```

Also, for JSON lines we skip the echo. Modify the echo section (the `UART1_SendData` call) to only echo non-JSON chars. Find:

```c
        /* Echo back */
        UART1_SendData((uint8_t *)&c, 1);
```

Replace with condition — but actually this is tricky without knowing if we're in a JSON line. Simpler approach: disable echo when buffer starts with '{', handle it in buffer building. Let's take a cleaner approach:

Add a static flag `g_in_json` to track when we're buffering a JSON line. Find the static variables at the top:

```c
static char g_cmd_buf[CMD_BUF_SIZE];
static uint8_t g_cmd_len = 0;
static uint16_t g_cooldown = 0;
```

Add:

```c
static uint8_t g_in_json = 0;  /* 1 = current line starts with '{' */
```

Then modify the character processing loop in `Cmd_Process`. The current loop:

```c
    for (uint16_t i = 0; i < rx_num; i++) {
        char c = rx_data[i];

        /* Echo back */
        UART1_SendData((uint8_t *)&c, 1);

        if (c == '\r' || c == '\n') {
            if (g_cmd_len > 0) {
                g_cmd_buf[g_cmd_len] = '\0';
                UART1_SendString("\r\n");
                Cmd_Execute(g_cmd_buf);
                g_cmd_len = 0;
            }
        } else if (c == '\b' || c == 0x7F) {
            ...
```

Replace the entire loop with:

```c
    for (uint16_t i = 0; i < rx_num; i++) {
        char c = rx_data[i];

        if (c == '\r' || c == '\n') {
            if (g_cmd_len > 0) {
                g_cmd_buf[g_cmd_len] = '\0';

                if (g_in_json) {
                    /* Process JSON line silently */
                    int8_t result = PC_Monitor_Parse(g_cmd_buf, g_cmd_len);
                    if (result == 1) {
                        extern volatile uint32_t g_sys_tick_ms;
                        g_pc_data.last_update_ms = g_sys_tick_ms;
                    }
                    g_in_json = 0;
                } else {
                    /* Regular command — echo newline then execute */
                    UART1_SendString("\r\n");
                    Cmd_Execute(g_cmd_buf);
                }
                g_cmd_len = 0;
            }
        } else if (c == '\b' || c == 0x7F) {
            if (g_cmd_len > 0) {
                g_cmd_len--;
                if (!g_in_json) {
                    UART1_SendString("\b \b");
                }
            }
        } else if (g_cmd_len < CMD_BUF_SIZE - 1 && c >= ' ') {
            if (g_cmd_len == 0 && c == '{') {
                g_in_json = 1;
            }
            g_cmd_buf[g_cmd_len++] = c;
            if (!g_in_json) {
                /* Echo human commands */
                UART1_SendData((uint8_t *)&c, 1);
            }
        }
    }
```

- [ ] **Step 4: Update help text to include new commands**

In `Cmd_Execute`, find the help section and update:

Replace the help content block with the same commands plus new ones. After `" save    - Force save\r\n"` add:

```c
        UART1_SendString(" pc      - PC monitor mode\r\n");
        UART1_SendString(" pet     - Pet game mode\r\n");
```

---

### Task 13: Modify main.c — Integrate PC monitor

**Files:**
- Modify: `D:\Apps\pixel_pet\User\main.c`

- [ ] **Step 1: Add pc_monitor.h include**

Add after existing includes in [main.c](D:\Apps\pixel_pet\User\main.c):

```c
#include "pc_monitor.h"
```

- [ ] **Step 2: Initialize PC monitor**

In `main()`, add after `Pet_Init();`:

```c
    /* PC Monitor init */
    PC_Monitor_Init();
```

- [ ] **Step 3: Update last_update_ms tracking in main loop**

In the main loop, after `Cmd_Process();`, add staleness check:

After:
```c
        /* --- Serial command processing --- */
        Cmd_Process();
```

Add:
```c
        /* --- PC data staleness check (3s timeout) --- */
        if (g_sys_tick_ms - g_pc_data.last_update_ms > 3000) {
            g_pc_data.is_connected = 0;
        }
```

- [ ] **Step 4: Gate pet-mode keys on !is_pc_mode**

In the key handling section, modify KEY1, KEY2, and KEY3 handlers to only work in pet mode.

Replace the KEY1 handler (feed):
```c
            /* KEY1: Feed */
            if (Key_Scan(KEY_1_GPIO, KEY_1_PIN, 1)) {
                if (!Cmd_IsCooldown()) {
                    Pet_Feed();
                    Render_ShowCommandResult("Fed! (K1)");
                    Pet_Save();
                }
            }
```

With:
```c
            /* KEY1: Feed (pet mode only) */
            if (Key_Scan(KEY_1_GPIO, KEY_1_PIN, 1)) {
                if (!g_pet.is_pc_mode && !Cmd_IsCooldown()) {
                    Pet_Feed();
                    Render_ShowCommandResult("Fed! (K1)");
                    Pet_Save();
                }
            }
```

Replace the KEY2 handler (play):
```c
            /* KEY2: Play */
            if (Key_Scan(KEY_2_GPIO, KEY_2_PIN, 0)) {
                if (!Cmd_IsCooldown()) {
                    Pet_Play();
                    Render_ShowCommandResult("Play! (K2)");
                    Pet_Save();
                }
            }
```

With:
```c
            /* KEY2: Play (pet mode only) */
            if (Key_Scan(KEY_2_GPIO, KEY_2_PIN, 0)) {
                if (!g_pet.is_pc_mode && !Cmd_IsCooldown()) {
                    Pet_Play();
                    Render_ShowCommandResult("Play! (K2)");
                    Pet_Save();
                }
            }
```

Replace the KEY3 handler (clean):
```c
            /* KEY3: Clean */
            if (Key_Scan(KEY_3_GPIO, KEY_3_PIN, 0)) {
                if (!Cmd_IsCooldown()) {
                    Pet_Clean();
                    Render_ShowCommandResult("Clean! (K3)");
                    Pet_Save();
                }
            }
```

With:
```c
            /* KEY3: Clean (pet mode only) */
            if (Key_Scan(KEY_3_GPIO, KEY_3_PIN, 0)) {
                if (!g_pet.is_pc_mode && !Cmd_IsCooldown()) {
                    Pet_Clean();
                    Render_ShowCommandResult("Clean! (K3)");
                    Pet_Save();
                }
            }
```

- [ ] **Step 5: Add mode LED indicator**

In the 100ms tick section, after the existing LED sickness logic, add PC mode LED:

After the existing LED block:
```c
            /* LED indication */
            if (g_pet.is_sick) { ... } else { ... }
```

Add:
```c
            /* PC mode LED: blue = PC monitor mode */
            if (g_pet.is_pc_mode) {
                LED_RED_OFF;
                /* Blue LED steady on in PC mode */
                LED_BLUE_ON;
            } else if (!g_pet.is_sick) {
                LED_BLUE_OFF;
            }
```

- [ ] **Step 6: Update welcome screen for new project name**

In `main()`, update the welcome screen text from "Pixel Slime Pet" to "Pixel Claude Pet":

Find:
```c
    LCD_String(30, 180, "Pixel Slime Pet", 16, WHITE, BLACK);
```

Replace with:
```c
    LCD_String(30, 180, "Pixel Claude Pet", 16, WHITE, BLACK);
```

And find:
```c
    UART1_SendString("  Pixel Slime Pet v1.0\r\n");
```

Replace with:
```c
    UART1_SendString("  Pixel Claude Pet v2.0\r\n");
```

Also update the `pet_command.c` Cmd_Init welcome message in the same way.

---

## Phase 3: Claude Code Hooks

### Task 14: Configure Claude Code Hooks for status file

**Files:**
- Modify: `C:\Users\29918\.claude\settings.json` (if hooks are configured there)

- [ ] **Step 1: Document hook configuration**

Claude Code hooks can be configured in `~/.claude/settings.json`. Add the following documentation/reference for the user to configure:

```json
{
  "hooks": {
    "PostToolUse": [
      {
        "matcher": "",
        "command": "echo idle > C:\\Users\\29918\\.claude\\claude_status.txt"
      }
    ],
    "Notification": [
      {
        "matcher": "thinking",
        "command": "echo thinking > C:\\Users\\29918\\.claude\\claude_status.txt"
      }
    ]
  }
}
```

**Note:** Claude Code hook events may not cover all states natively. A practical approach:
- Use a scheduled task or the service itself to detect Claude Code activity via process monitoring
- OR: The user manually updates the status file when needed
- OR: Write a small batch script that Claude Code invokes at appropriate times

For now, the service reads `claude_status.txt`. The file can be written by:
1. Manual echo commands during development
2. A future Claude Code integration
3. The `status_reader.py` defaults to `idle` when no file exists

- [ ] **Step 2: Create test status file**

Run: `echo idle > C:\Users\29918\.claude\claude_status.txt`

Verify: `type C:\Users\29918\.claude\claude_status.txt`
Expected: `idle`

---

## Phase 4: Build & Test

### Task 15: Add source files to Keil project

**Files:**
- Modify: `D:\Apps\pixel_pet\STM32F407.uvprojx` (Keil project)

- [ ] **Step 1: Open Keil MDK and add pc_monitor.c to the APP group**

Instructions:
1. Open `D:\Apps\pixel_pet\STM32F407.uvprojx` in Keil MDK
2. Right-click the "APP" group → "Add Existing Files to Group 'APP'"
3. Select `D:\Apps\pixel_pet\app\pc_monitor.c`
4. Click OK

- [ ] **Step 2: Add include path for pc_monitor.h**

The app directory is already in the include path (check in Keil → Project Options → C/C++ → Include Paths). If not, add `..\app`.

- [ ] **Step 3: Compile the project**

Press F7 in Keil, or use command line:
Expected: 0 Errors, 0 Warnings.

- [ ] **Step 4: Flash to STM32F407VET6**

Press F8 in Keil.
Expected: Flash successful.

---

### Task 16: End-to-end test

- [ ] **Step 1: Start PC monitor in debug mode**

```bash
cd D:\Apps\pixel_pet\pc_host
python pc_monitor_service.py
```

- [ ] **Step 2: Verify serial data arrives at STM32**

Connect a serial terminal to COM3 at 115200, you should see JSON lines being sent.
Or: watch the STM32 LCD — it should show PC status bars.

- [ ] **Step 3: Test Claude status changes**

```bash
echo thinking > C:\Users\29918\.claude\claude_status.txt
```

Observe LCD: Claude character should change expression to "thinking" and show "Thinking..." text.

- [ ] **Step 4: Test mode switch**

Send `pet` command via serial terminal → LCD should switch to pet game.
Send `pc` command → LCD should switch back to PC monitor.

- [ ] **Step 5: Test error handling**

Unplug serial cable → LCD should show "NO SIGNAL" after 3 seconds.
Plug back in → should reconnect and resume.

---

## Task Dependency Graph

```
Task 1 (config) ──┐
                  ├── Task 5 (service) ── Task 6 (batch)
Task 2 (sysmon) ──┤
Task 3 (status) ──┤
Task 4 (serial) ──┘

Task 7 (pc_monitor.h) ── Task 8 (pc_monitor.c) ──┐
                                                   ├── Task 12 (commands) ── Task 13 (main) ── Task 15 (build)
Task 9  (pet_core.h mod) ─────────────────────────┤
Task 10 (pet_render.h mod) ── Task 11 (render.c) ─┘

Task 14 (hooks) ── after Task 3

Task 16 (E2E test) ── after Phase 1 + Phase 2 complete
```

**PC side (Tasks 1-6) and STM32 side (Tasks 7-13) can be developed in parallel.**
