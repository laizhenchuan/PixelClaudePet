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

    def read_line(self) -> str:
        """Read a line from serial. Returns empty string if nothing available."""
        if self._ser is None or not self._ser.is_open:
            return ""
        try:
            return self._ser.readline().decode('utf-8', errors='ignore').strip()
        except (serial.SerialException, OSError):
            return ""

    def close(self):
        if self._ser and self._ser.is_open:
            self._ser.close()
        self._ser = None

    @property
    def is_connected(self) -> bool:
        return self._ser is not None and self._ser.is_open
