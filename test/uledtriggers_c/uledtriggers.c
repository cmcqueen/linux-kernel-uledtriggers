// SPDX-License-Identifier: GPL-2.0
/*
 * uledtriggers.c
 *
 * This program creates a new userspace LED trigger and monitors it. A
 * timestamp and brightness value is printed each time the brightness changes.
 *
 * Usage: uledtriggers <trigger-name>
 *
 * <trigger-name> is the name of the LED trigger to be created. Pressing
 * CTRL+C will exit.
 */

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <linux/uledtriggers.h>

int main(int argc, char const *argv[])
{
	struct uledtriggers_user_dev uledtriggers_dev;
	int fd, ret;
	int brightness;
	struct uledtriggers_blink blink = { .delay_on = 200, .delay_off = 200 };
	struct uledtriggers_blink_oneshot blink_oneshot = { .delay_on = 100, .delay_off = 200, .invert = 0, .__unused = 0 };

	if (argc != 2) {
		fprintf(stderr, "Requires <trigger-name> argument\n");
		return 1;
	}

	strncpy(uledtriggers_dev.name, argv[1], LED_TRIGGER_MAX_NAME_SIZE);

	fd = open("/dev/uledtriggers", O_RDWR);
	if (fd == -1) {
		perror("Failed to open /dev/uledtriggers");
		return 1;
	}

	#if 0
	// Setup by write.
	ret = write(fd, &uledtriggers_dev, sizeof(uledtriggers_dev));
#else
	// Setup by ioctl.
	ret = ioctl(fd, ULEDTRIGGERS_IOC_DEV_SETUP, &uledtriggers_dev);
#endif
	if (ret == -1) {
		perror("Failed to write to /dev/uledtriggers");
		close(fd);
		return 1;
	}

	while (1) {
		// Change brightness via write.
		brightness = ULEDTRIGGERS_FULL;
		ret = write(fd, &brightness, sizeof(brightness));
		if (ret < 0)
			goto error;
		usleep(2000000);

		brightness = ULEDTRIGGERS_OFF;
		ret = write(fd, &brightness, sizeof(brightness));
		if (ret < 0)
			goto error;
		usleep(2000000);

		// Change brightness via ioctl.
		brightness = ULEDTRIGGERS_FULL;
		ret = ioctl(fd, ULEDTRIGGERS_IOC_EVENT, &brightness);
		if (ret)
			goto error;
		usleep(1000000);

		brightness = ULEDTRIGGERS_OFF;
		ret = ioctl(fd, ULEDTRIGGERS_IOC_EVENT, &brightness);
		if (ret)
			goto error;
		usleep(1000000);

		// Set up continuous blink.
		ret = ioctl(fd, ULEDTRIGGERS_IOC_BLINK, &blink);
		if (ret)
			goto error;
		usleep(1000000);
		ret = ioctl(fd, ULEDTRIGGERS_IOC_OFF, 0);
		if (ret)
			goto error;
		usleep(1000000);

		// Set up single blink.
		ret = ioctl(fd, ULEDTRIGGERS_IOC_OFF, 0);
		if (ret)
			goto error;
		ret = ioctl(fd, ULEDTRIGGERS_IOC_BLINK_ONESHOT, &blink_oneshot);
		if (ret)
			goto error;
		usleep(1000000);
	}

	close(fd);

	return 0;

error:
	perror("Failed");
	return 1;
}
