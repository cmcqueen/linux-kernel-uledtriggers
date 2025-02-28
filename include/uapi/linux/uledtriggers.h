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

#define LED_TRIGGER_MAX_NAME_SIZE	64

struct uledtriggers_user_dev {
	char name[LED_TRIGGER_MAX_NAME_SIZE];
	int max_brightness;
};

#endif /* _UAPI__ULEDTRIGGERS_H_ */
