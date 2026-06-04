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

try:
    import pyautogui
    HAS_PYAUTOGUI = True
except ImportError:
    HAS_PYAUTOGUI = False

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

            # Check for key commands from STM32
            line = self._sender.read_line()
            if line.startswith("KEY:"):
                key = line[4:].strip()
                logging.info("Key received: %s", key)
                if HAS_PYAUTOGUI:
                    try:
                        if key == "YES":
                            pyautogui.write("yes", interval=0.02)
                            pyautogui.press("enter")
                            logging.info("Typed: yes")
                        elif key == "NO":
                            pyautogui.write("no", interval=0.02)
                            pyautogui.press("enter")
                            logging.info("Typed: no")
                    except Exception as e:
                        logging.error("pyautogui failed: %s", e)

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
                    # Check for key commands from STM32
                    line = sender.read_line()
                    if line.startswith("KEY:"):
                        key = line[4:].strip()
                        print(f"\n[KEY] {key}")
                        if HAS_PYAUTOGUI:
                            try:
                                if key == "YES":
                                    pyautogui.write("yes", interval=0.02)
                                    pyautogui.press("enter")
                                elif key == "NO":
                                    pyautogui.write("no", interval=0.02)
                                    pyautogui.press("enter")
                            except Exception as e:
                                print(f"pyautogui error: {e}")
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
