#!/usr/bin/python3
"""
uledtriggers_blink_steady.py

This program creates a userspace LED trigger named "blink-steady", and tests a transition from
blinking to steady-on.

It blinks the LED for 5 seconds, and then leaves it steady-on for 5 seconds, and repeats.

For some Linux kernels with LEDs that use software blinking, the kernel LED subsystem does not
correctly turn the LED on when it is set to steady-on immediately after blinking. This program
aims to demonstrate this. If the LED is left steady-off for 5 seconds rather than steady-on,
this shows an issue with the Linux kernel LED subsystem.
https://lore.kernel.org/linux-leds/20260514164031.GQ305027@google.com/T/#t

Usage: ./uledtriggers_blink_steady.py
"""

from enum import IntEnum
import fcntl
import struct
import time

# 3rd-party packages
from ioctl_opt import IOC, IOC_WRITE

ledtrigger_name = 'blink-steady'

CHAR_DEV_NAME = '/dev/uledtriggers'

class EnumMissingUnknown():
    @classmethod
    def _missing_(cls, value):
        return cls.Unknown

class IOCTL(EnumMissingUnknown, IntEnum):
    Unknown        = -1
    ULEDTRIGGERS_IOC_DEV_SETUP      = IOC(IOC_WRITE, ord('t'), 0x01, 64)
    ULEDTRIGGERS_IOC_OFF            = IOC(0, ord('t'), 0x10, 0)
    ULEDTRIGGERS_IOC_ON             = IOC(0, ord('t'), 0x11, 0)
    ULEDTRIGGERS_IOC_EVENT          = IOC(IOC_WRITE, ord('t'), 0x12, 4)
    ULEDTRIGGERS_IOC_BLINK          = IOC(IOC_WRITE, ord('t'), 0x20, 16)
    ULEDTRIGGERS_IOC_BLINK_ONESHOT  = IOC(IOC_WRITE, ord('t'), 0x21, 24)


def uledtrigger_register(name):
    f = open(CHAR_DEV_NAME, 'r+b', buffering=0)
    setup_struct = struct.pack('@64s', name.encode('utf-8'))
    fcntl.ioctl(f, IOCTL.ULEDTRIGGERS_IOC_DEV_SETUP, setup_struct)
    return f

def brightness_bytes(brightness=0):
    return struct.pack('@i', brightness)

def main():
    uledtrigger_f = uledtrigger_register(ledtrigger_name)
    try:
        while True:
            # Set up continuous blink.
            blink_struct = struct.pack('@QQ', 200, 200)
            fcntl.ioctl(uledtrigger_f, IOCTL.ULEDTRIGGERS_IOC_BLINK, blink_struct)
            time.sleep(5)

            # Go to steady-on.
            # First set brightness = 0 to stop the continuous blink.
            fcntl.ioctl(uledtrigger_f, IOCTL.ULEDTRIGGERS_IOC_EVENT, brightness_bytes(0))
            # If the LED doesn't go steady-on, enabling the following line is a work-around.
            # time.sleep(0.1)
            fcntl.ioctl(uledtrigger_f, IOCTL.ULEDTRIGGERS_IOC_EVENT, brightness_bytes(255))
            time.sleep(5)

    finally:
        uledtrigger_f.close()

if __name__ == '__main__':
    main()
