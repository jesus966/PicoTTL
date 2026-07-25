# Copyright (c) 2026 Jesús Fernández Gamito
# SPDX-License-Identifier: MIT

"""Minimal USB CDC transport for PicoTTL Example 43.

Protocol (see the firmware header):
  'H'                    -> one-line hello with the mode table
  'M' + mode index byte  -> "OK <fbBytes>" or "ERR ..."
  'F' + u32le length     -> raw framebuffer bytes, then "ACK"/"ERR ..."
"""

import struct

import serial


class PicoLink:
    def __init__(self, port: str, timeout: float = 5.0):
        # Baud rate is ignored by USB CDC.
        self._serial = serial.Serial(port, baudrate=115200,
                                     timeout=timeout)

    def _read_line(self) -> str:
        line = self._serial.readline().decode(errors="replace").strip()
        if not line:
            raise TimeoutError("no response from firmware")
        return line

    def hello(self) -> str:
        self._serial.reset_input_buffer()
        self._serial.write(b"H")
        return self._read_line()

    def set_mode(self, index: int) -> int:
        """Selects a mode; returns the expected framebuffer size."""
        self._serial.write(b"M" + bytes([index]))
        reply = self._read_line()
        if not reply.startswith("OK "):
            raise RuntimeError(f"mode switch failed: {reply}")
        return int(reply.split()[1])

    def send_frame(self, framebuffer: bytes) -> None:
        self._serial.write(b"F" + struct.pack("<I", len(framebuffer)))
        self._serial.write(framebuffer)
        reply = self._read_line()
        if reply != "ACK":
            raise RuntimeError(f"frame rejected: {reply}")

    def close(self) -> None:
        self._serial.close()
