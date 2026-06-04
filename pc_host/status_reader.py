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
