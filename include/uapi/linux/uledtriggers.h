/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Userspace LED triggers driver support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _UAPI__ULEDTRIGGERS_H_
#define _UAPI__ULEDTRIGGERS_H_

/* See TRIG_NAME_MAX in linux/leds.h */
#define LED_TRIGGER_MAX_NAME_SIZE	50

/*
 * Struct for initial write to setup, or ioctl ULEDTREGGERS_IOC_DEV_SETUP.
 */
struct uledtriggers_user_dev {
	char name[LED_TRIGGER_MAX_NAME_SIZE];
};

/*
 * Brightness levels for writes of int values, or for use with ULEDTRIGGERS_IOC_EVENT.
 * These correspond to Linux kernel internal enum led_brightness in linux/leds.h.
 */
enum uledtriggers_brightness {
	ULEDTRIGGERS_OFF		= 0,
	ULEDTRIGGERS_ON			= 1,
	ULEDTRIGGERS_HALF		= 127,
	ULEDTRIGGERS_FULL		= 255,
};

/*
 * Struct for ioctl ULEDTRIGGERS_IOC_BLINK.
 */
struct uledtriggers_blink {
	unsigned long delay_on;
	unsigned long delay_off;
};

/*
 * Struct for ioctl ULEDTRIGGERS_IOC_BLINK_ONESHOT.
 * Note padding at the end due to alignment (for 64-bit kernels). Ensure it's set to 0.
 */
struct uledtriggers_blink_oneshot {
	unsigned long delay_on;
	unsigned long delay_off;
	int invert;
	int __unused;
};


/* ioctl commands */

#define ULEDTRIGGERS_IOC_MAGIC			't'

/*
 * Initial setup.
 * E.g.:
 *	int retval;
 *	struct uledtriggers_user_dev dev_setup = { "transmogrifier" };
 *	retval = ioctl(fd, ULEDTRIGGERS_IOC_DEV_SETUP, &dev_setup);
 */
#define ULEDTRIGGERS_IOC_DEV_SETUP	_IOW(ULEDTRIGGERS_IOC_MAGIC, 0x01, struct uledtriggers_user_dev)

/*
 * Turn the trigger off.
 * E.g.:
 *	int retval;
 *	retval = ioctl(fd, ULEDTRIGGERS_IOC_OFF);
 */
#define ULEDTRIGGERS_IOC_OFF		_IO(ULEDTRIGGERS_IOC_MAGIC, 0x10)

/*
 * Turn the trigger on.
 * E.g.:
 *	int retval;
 *	retval = ioctl(fd, ULEDTRIGGERS_IOC_ON);
 */
#define ULEDTRIGGERS_IOC_ON		_IO(ULEDTRIGGERS_IOC_MAGIC, 0x11)

/*
 * Set the LED trigger to a specified brightness.
 * Refer to enum uledtriggers_brightness.
 * E.g.:
 *	int retval;
 *	int brightness = ULEDTRIGGERS_FULL;
 *	retval = ioctl(fd, ULEDTRIGGERS_IOC_EVENT, &brightness);
 */
#define ULEDTRIGGERS_IOC_EVENT		_IOW(ULEDTRIGGERS_IOC_MAGIC, 0x12, int)

/*
 * Set the LED trigger to blink continuously.
 * E.g.:
 *	int retval;
 *	struct uledtriggers_blink blink;
 *      blink.delay_on = 100;
 *      blink.delay_off = 400;
 *	retval = ioctl(fd, ULEDTRIGGERS_IOC_BLINK, &blink);
 */
#define ULEDTRIGGERS_IOC_BLINK		_IOW(ULEDTRIGGERS_IOC_MAGIC, 0x20, struct uledtriggers_blink)

/*
 * Set the LED trigger to blink once.
 * E.g.:
 *	int retval;
 *	struct uledtriggers_blink_oneshot blink_oneshot;
 *      blink_oneshot.delay_on = 100;
 *      blink_oneshot.delay_off = 400;
 *      blink_oneshot.invert = false;
 *      blink_oneshot.__unused = 0;
 *	retval = ioctl(fd, ULEDTRIGGERS_IOC_BLINK_ONESHOT, &blink_oneshot);
 */
#define ULEDTRIGGERS_IOC_BLINK_ONESHOT	_IOW(ULEDTRIGGERS_IOC_MAGIC, 0x21, struct uledtriggers_blink_oneshot)


#endif /* _UAPI__ULEDTRIGGERS_H_ */
