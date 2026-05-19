// SPDX-License-Identifier: GPL-2.0
/*
 * uledtriggers_blink_steady.c
 *
 * This program creates a userspace LED trigger named "blink-steady", and tests a transition from
 * blinking to steady-on.
 *
 * It blinks the LED for 5 seconds, and then leaves it steady-on for 5 seconds, and repeats.
 *
 * For some Linux kernels with LEDs that use software blinking, the kernel LED subsystem does not
 * correctly turn the LED on when it is set to steady-on immediately after blinking. This program
 * aims to demonstrate this. If the LED is left steady-off for 5 seconds rather than steady-on,
 * this shows an issue with the Linux kernel LED subsystem.
 * https://lore.kernel.org/linux-leds/20260514164031.GQ305027@google.com/T/#t
 *
 * Usage: uledtriggers_blink_steady
 *
 * Pressing CTRL+C will exit.
 */

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <linux/uledtriggers.h>

int main(int, char const *[])
{
	struct uledtriggers_user_dev uledtriggers_dev = {};
	int fd, ret;
	int brightness;
	struct uledtriggers_blink blink = { .delay_on = 200, .delay_off = 200 };

	strncpy(uledtriggers_dev.name, "blink-steady", LED_TRIGGER_MAX_NAME_SIZE);

	fd = open("/dev/uledtriggers", O_RDWR);
	if (fd == -1) {
		perror("Failed to open /dev/uledtriggers");
		return 1;
	}

	// Setup by ioctl.
	ret = ioctl(fd, ULEDTRIGGERS_IOC_DEV_SETUP, &uledtriggers_dev);
	if (ret == -1) {
		perror("Failed to set up /dev/uledtriggers");
		close(fd);
		return 1;
	}

	while (1) {
		// Set up continuous blink.
		ret = ioctl(fd, ULEDTRIGGERS_IOC_BLINK, &blink);
		if (ret)
			goto error;
		usleep(5000000);

		// Go to steady-on.
		// First set brightness = 0 to stop the continuous blink.
		brightness = 0;
		ret = ioctl(fd, ULEDTRIGGERS_IOC_EVENT, &brightness);
		if (ret)
			goto error;
		// If the LED doesn't go steady-on, enabling the following line is a work-around.
		//usleep(100000);
		brightness = 255;
		ret = ioctl(fd, ULEDTRIGGERS_IOC_EVENT, &brightness);
		if (ret)
			goto error;
		usleep(5000000);
	}

	close(fd);

	return 0;

error:
	perror("Failed");
	return 1;
}
