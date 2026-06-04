"""System hardware monitor — reads CPU/GPU/Memory stats."""
import subprocess
import time
import psutil

try:
    import GPUtil
    HAS_GPU = True
except ImportError:
    HAS_GPU = False


def _get_cpu_temp_wmi() -> float:
    """Read CPU temperature via Windows WMI (MSAcpi_ThermalZoneTemperature).
    Returns Celsius, or -1 on failure."""
    try:
        out = subprocess.check_output(
            ['wmic', '/namespace:\\\\root\\wmi', 'PATH',
             'MSAcpi_ThermalZoneTemperature', 'get', 'CurrentTemperature', '/value'],
            timeout=3, encoding='utf-8', errors='ignore'
        )
        for line in out.splitlines():
            line = line.strip()
            if line.startswith('CurrentTemperature='):
                val = int(line.split('=')[1])
                # WMI returns temperature in Kelvin * 10
                return (val / 10.0) - 273.15
    except Exception:
        pass
    return -1.0


def get_cpu_temp() -> float:
    """Get CPU package temperature in Celsius. Returns -1 if unavailable."""
    # Windows: use WMI
    result = _get_cpu_temp_wmi()
    if result > 0:
        return result

    # Linux fallback: psutil
    try:
        temps = psutil.sensors_temperatures()
        if 'coretemp' in temps:
            for entry in temps['coretemp']:
                if 'package' in entry.label.lower():
                    return entry.current
            vals = [e.current for e in temps['coretemp']]
            if vals:
                return sum(vals) / len(vals)
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
        'time': time.strftime('%H:%M:%S'),
        'cpu_temp': get_cpu_temp(),
        'cpu_usage': get_cpu_usage(),
        'gpu_temp': get_gpu_temp(),
        'gpu_usage': get_gpu_usage(),
        'mem_usage': get_mem_usage(),
    }
