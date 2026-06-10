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

# Handle both frozen EXE and script modes
if getattr(sys, 'frozen', False):
    # Try EXE dir, then parent dir (dist/ → pc_host/)
    exe_dir = Path(sys.executable).parent
    if (exe_dir / 'config.json').exists():
        SCRIPT_DIR = exe_dir
    else:
        SCRIPT_DIR = exe_dir.parent
else:
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
                try:
                    import ctypes
                    VK_MEDIA_PLAY_PAUSE = 0xB3
                    VK_MEDIA_NEXT_TRACK = 0xB0
                    VK_MEDIA_PREV_TRACK = 0xB1

                    vk = None
                    if key == "PAUSE":
                        vk = VK_MEDIA_PLAY_PAUSE
                    elif key == "NEXT":
                        vk = VK_MEDIA_NEXT_TRACK
                    elif key == "PREV":
                        vk = VK_MEDIA_PREV_TRACK

                    if vk is not None:
                        ctypes.windll.user32.keybd_event(vk, 0, KEYEVENTF_EXTENDEDKEY, 0)
                        ctypes.windll.user32.keybd_event(vk, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0)
                        logging.info("Media key sent: %s", key)
                except Exception as e:
                    logging.error("Media key failed: %s", e)

            # Sleep until next interval or stop
            win32event.WaitForSingleObject(self._stop_event, int(interval * 1000))


# Allow running as regular script for debugging
if __name__ == '__main__':
    _is_pythonw = 'pythonw' in sys.executable.lower()
    if len(sys.argv) == 1 and not getattr(sys, 'frozen', False) and not _is_pythonw:
        # Script mode with console: run in foreground
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
        print("Pixel Claude Pet — running (Ctrl+C to stop)")
        try:
            while True:
                data = collect_all()
                data['claude'] = status_reader.read()
                data_str = json.dumps(data, ensure_ascii=False)
                print(f"\r{data_str}", end='', flush=True)
                if sender.is_connected:
                    sender.send(data)
                    line = sender.read_line()
                    if line.startswith("KEY:"):
                        key = line[4:].strip()
                        try:
                            import ctypes
                            VK = {'PAUSE':0xB3,'NEXT':0xB0,'PREV':0xB1}
                            vk = VK.get(key)
                            if vk:
                                ctypes.windll.user32.keybd_event(vk,0,1,0)
                                ctypes.windll.user32.keybd_event(vk,0,3,0)
                        except: pass
                elif not sender.connect():
                    print(f"\n[!] Waiting for {config['serial']['port']}...")
                    time.sleep(5)
                    continue
                time.sleep(interval)
        except KeyboardInterrupt:
            print("\nStopped.")
            sender.close()
    elif getattr(sys, 'frozen', False) or _is_pythonw:
        # EXE or pythonw: silent background mode
        import win32event, win32api, winerror
        mutex = win32event.CreateMutex(None, False, 'PixelClaudePet_SingleInstance')
        if win32api.GetLastError() == winerror.ERROR_ALREADY_EXISTS:
            sys.exit(0)

        # EXE mode: run as service-like background loop without console
        config = load_config()
        log_file = config.get('service', {}).get('log_file', 'pixel_claude_pet.log')
        logging.basicConfig(filename=log_file, level=logging.INFO,
                            format='%(asctime)s %(message)s')
        sender = SerialSender(config['serial']['port'], config['serial']['baudrate'],
                              config['serial'].get('timeout', 1.0))
        status_reader = StatusReader(config['claude']['status_file'],
                                     config['claude']['default_status'])
        interval = config['monitor']['interval_sec']
        logging.info("Pixel Claude Pet EXE started")
        while True:
            try:
                data = collect_all()
                data['claude'] = status_reader.read()
                if not sender.is_connected:
                    sender.connect()
                sender.send(data)
                line = sender.read_line()
                if line.startswith("KEY:"):
                    key = line[4:].strip()
                    try:
                        import ctypes
                        VK = {'PAUSE':0xB3,'NEXT':0xB0,'PREV':0xB1}
                        vk = VK.get(key)
                        if vk:
                            ctypes.windll.user32.keybd_event(vk,0,1,0)
                            ctypes.windll.user32.keybd_event(vk,0,3,0)
                    except: pass
                time.sleep(interval)
            except Exception as e:
                logging.error(str(e))
                time.sleep(5)
    else:
        win32serviceutil.HandleCommandLine(PcMonitorService)
