#!/usr/bin/python3

from enum import IntEnum
import fcntl
import struct
import time

ledtrigger_names = ('utest0', 'utest1')

CHAR_DEV_NAME = '/dev/uledtriggers'

class EnumMissingUnknown():
    @classmethod
    def _missing_(cls, value):
        return cls.Unknown

class IOCTL(EnumMissingUnknown, IntEnum):
    Unknown        = -1
    ULEDTRIGGERS_IOC_DEV_SETUP      = 0x40327401
    ULEDTRIGGERS_IOC_OFF            = 0x00007410
    ULEDTRIGGERS_IOC_ON             = 0x00007411
    ULEDTRIGGERS_IOC_EVENT          = 0x40047412
    ULEDTRIGGERS_IOC_BLINK          = 0x40107420
    ULEDTRIGGERS_IOC_BLINK_ONESHOT  = 0x40187421

def uledtriggers_register(names):
    uledtriggers_f = []
    for name in names:
        f = open(CHAR_DEV_NAME, 'r+b', buffering=0)
        name_struct = struct.pack('@50s', name.encode('utf-8'))
        if False:
            f.write(name_struct)
        else:
            fcntl.ioctl(f, IOCTL.ULEDTRIGGERS_IOC_DEV_SETUP, name_struct)
        uledtriggers_f.append((name, f))
    return uledtriggers_f

def uledtriggers_blink(uledtriggers_f):
    for i, (_name, f) in enumerate(uledtriggers_f):
        blink_struct = struct.pack('@QQ', 1000, 2000)
        fcntl.ioctl(f, IOCTL.ULEDTRIGGERS_IOC_BLINK, blink_struct)
    while True:
        time.sleep(1)

def uledtriggers_periodic_blink_oneshot(uledtriggers_f):
    while True:
        time.sleep(2)
        for i, (_name, f) in enumerate(uledtriggers_f):
            invert = 1 if i & 1 else 0
            blink_struct = struct.pack('@QQIxxxx', 200, 300, invert)
            fcntl.ioctl(f, IOCTL.ULEDTRIGGERS_IOC_BLINK_ONESHOT, blink_struct)

def uledtriggers_periodic_toggle_on_off(uledtriggers_f):
    while True:
        time.sleep(2)
        for i, (_name, f) in enumerate(uledtriggers_f):
            fcntl.ioctl(f, IOCTL.ULEDTRIGGERS_IOC_ON if i & 1 else IOCTL.ULEDTRIGGERS_IOC_OFF)
        time.sleep(2)
        for i, (_name, f) in enumerate(uledtriggers_f):
            fcntl.ioctl(f, IOCTL.ULEDTRIGGERS_IOC_OFF if i & 1 else IOCTL.ULEDTRIGGERS_IOC_ON)

def uledtriggers_periodic_toggle_event(uledtriggers_f):
    while True:
        time.sleep(2)
        for i, (_name, f) in enumerate(uledtriggers_f):
            brightness = 255 if i & 1 else 0
            brightness_bytes = struct.pack('@i', brightness)
            fcntl.ioctl(f, IOCTL.ULEDTRIGGERS_IOC_EVENT, brightness_bytes)
        time.sleep(2)
        for i, (_name, f) in enumerate(uledtriggers_f):
            brightness = 0 if i & 1 else 255
            brightness_bytes = struct.pack('@i', brightness)
            fcntl.ioctl(f, IOCTL.ULEDTRIGGERS_IOC_EVENT, brightness_bytes)

def uledtriggers_periodic_toggle_write(uledtriggers_f):
    while True:
        time.sleep(2)
        for i, (_name, f) in enumerate(uledtriggers_f):
            brightness = 255 if i & 1 else 0
            brightness_bytes = struct.pack('@i', brightness)
            f.write(brightness_bytes)
        time.sleep(2)
        for i, (_name, f) in enumerate(uledtriggers_f):
            brightness = 0 if i & 1 else 255
            brightness_bytes = struct.pack('@i', brightness)
            f.write(brightness_bytes)

def main():
    uledtriggers_f = uledtriggers_register(ledtrigger_names)
    try:
        uledtriggers_periodic_toggle_on_off(uledtriggers_f)
        #uledtriggers_periodic_toggle_event(uledtriggers_f)
        #uledtriggers_periodic_toggle_write(uledtriggers_f)
        #uledtriggers_blink(uledtriggers_f)
        #uledtriggers_periodic_blink_oneshot(uledtriggers_f)
    finally:
        for _name, f in uledtriggers_f:
            f.close()

if __name__ == '__main__':
    main()
