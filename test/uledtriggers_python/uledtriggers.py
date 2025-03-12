#!/usr/bin/python3

from enum import IntEnum
import fcntl
import struct
import time

# 3rd-party packages
from ioctl_opt import IOC, IOC_WRITE

ledtrigger_names = ('utest0', 'utest1')

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


def uledtriggers_register(names):
    uledtriggers_f = []
    for name in names:
        f = open(CHAR_DEV_NAME, 'r+b', buffering=0)
        setup_struct = struct.pack('@64s', name.encode('utf-8'))
        if False:
            f.write(setup_struct)
        else:
            fcntl.ioctl(f, IOCTL.ULEDTRIGGERS_IOC_DEV_SETUP, setup_struct)
        uledtriggers_f.append((name, f))
    return uledtriggers_f

def brightness_bytes(brightness=0):
    return struct.pack('@i', brightness)

def main():
    uledtriggers_f = uledtriggers_register(ledtrigger_names)
    try:
        while True:
            # Change brightness via write.
            for i, (_name, f) in enumerate(uledtriggers_f):
                f.write(brightness_bytes(255))
            time.sleep(2)
            for i, (_name, f) in enumerate(uledtriggers_f):
                f.write(brightness_bytes(0))
            time.sleep(2)

            # Change brightness via ioctl.
            for i, (_name, f) in enumerate(uledtriggers_f):
                fcntl.ioctl(f, IOCTL.ULEDTRIGGERS_IOC_EVENT, brightness_bytes(255))
            time.sleep(1)
            for i, (_name, f) in enumerate(uledtriggers_f):
                fcntl.ioctl(f, IOCTL.ULEDTRIGGERS_IOC_EVENT, brightness_bytes(0))
            time.sleep(1)

            # Set up continuous blink.
            blink_struct = struct.pack('@QQ', 200, 200)
            for i, (_name, f) in enumerate(uledtriggers_f):
                fcntl.ioctl(f, IOCTL.ULEDTRIGGERS_IOC_BLINK, blink_struct)
            time.sleep(1)
            for i, (_name, f) in enumerate(uledtriggers_f):
                fcntl.ioctl(f, IOCTL.ULEDTRIGGERS_IOC_OFF, 0)
            time.sleep(1)

            # Single blink
            blink_struct = struct.pack('@QQIxxxx', 100, 200, 0)
            for i, (_name, f) in enumerate(uledtriggers_f):
                fcntl.ioctl(f, IOCTL.ULEDTRIGGERS_IOC_OFF, 0)
                fcntl.ioctl(f, IOCTL.ULEDTRIGGERS_IOC_BLINK_ONESHOT, blink_struct)
            time.sleep(1)

    finally:
        for _name, f in uledtriggers_f:
            f.close()

if __name__ == '__main__':
    main()
