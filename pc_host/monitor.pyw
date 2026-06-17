"""Pixel Claude Pet — Silent background service"""
import time, json, logging, sys, os, subprocess
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

log_file = 'C:/Users/29918/pixel_claude_pet.log'
logging.basicConfig(filename=log_file, level=logging.INFO,
                    format='%(asctime)s %(message)s')
logging.info("Pixel Claude Pet started")

from serial_sender import SerialSender
from status_reader import StatusReader
from media_info import get_media_info, get_lyrics
import psutil

def _get_gpu_info():
    """Get GPU temp/usage via nvidia-smi, no console window."""
    try:
        si = subprocess.STARTUPINFO()
        si.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        out = subprocess.check_output(
            ['nvidia-smi', '--query-gpu=temperature.gpu,utilization.gpu',
             '--format=csv,noheader,nounits'],
            startupinfo=si, timeout=3, encoding='utf-8', errors='ignore')
        parts = out.strip().split(',')
        return int(parts[0].strip()), int(parts[1].strip())
    except: pass
    return -1, -1

def collect():
    gpu_temp, gpu_usage = _get_gpu_info()
    return {'time': time.strftime('%H:%M:%S'), 'date': time.strftime('%Y-%m-%d'),
            'weekday': time.localtime().tm_wday,
            'cpu_temp': -1.0, 'cpu_usage': int(psutil.cpu_percent(interval=0.1)),
            'gpu_temp': gpu_temp, 'gpu_usage': gpu_usage,
            'mem_usage': int(psutil.virtual_memory().percent)}

sender = SerialSender('COM3', 115200, 0.5)
sr = StatusReader('C:/Users/29918/.claude/claude_status.txt', 'idle')

_last_media = 0
_media_cache = {'title': '', 'artist': ''}

while True:
    try:
        data = collect()
        data['claude'] = sr.read()
        # Query media info every 3s
        if time.time() - _last_media > 3:
            _last_media = time.time()
            _media_cache = get_media_info()
        data['song'] = _media_cache['title']
        data['artist'] = _media_cache['artist']
        data['lyrics'] = get_lyrics(_media_cache['title'], _media_cache['artist'])[:256]
        if not sender.is_connected:
            sender.connect()
        sender.send(data)
        line = sender.read_line()
        if line.startswith("KEY:"):
            key = line[4:].strip()
            import ctypes
            VK = {'PAUSE': 0xB3, 'NEXT': 0xB0, 'PREV': 0xB1}
            vk = VK.get(key)
            if vk:
                ctypes.windll.user32.keybd_event(vk, 0, 1, 0)
                ctypes.windll.user32.keybd_event(vk, 0, 3, 0)
        time.sleep(1)
    except Exception as e:
        logging.error(str(e))
        time.sleep(3)
