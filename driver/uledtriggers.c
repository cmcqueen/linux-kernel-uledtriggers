// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Userspace LED triggers driver
 *
 * Copyright (C) 2025 Craig McQueen <craig@mcqueen.au>
 */
#include <linux/ctype.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/miscdevice.h>
#include <linux/module.h>

#include <uapi/linux/uledtriggers.h>

#define ULEDTRIGGERS_NAME	"uledtriggers"

enum uledtriggers_state {
	ULEDTRIGGERS_STATE_UNKNOWN,
	ULEDTRIGGERS_STATE_REGISTERED,
};

enum uledtriggers_trig_state {
	TRIG_STATE_EVENT,
	TRIG_STATE_BLINK,
};

struct uledtriggers_device {
	struct uledtriggers_user_dev	user_dev;
	struct led_trigger	led_trigger;
	struct mutex		mutex;
	enum uledtriggers_state	state;
	enum uledtriggers_trig_state	trig_state;
	int			brightness;
	unsigned long		trig_delay_on;
	unsigned long		trig_delay_off;
};

static struct miscdevice uledtriggers_misc;

/*
 * When an LED is connected to the trigger, this 'activate' function runs and
 * sets the initial state of the LED.
 */
static int uledtriggers_trig_activate(struct led_classdev *led_cdev)
{
	struct led_trigger		*trig;
	struct uledtriggers_device	*udev;
	enum uledtriggers_trig_state	trig_state;
	unsigned long			delay_on;
	unsigned long			delay_off;
	int				retval = 0;

	trig = led_cdev->trigger;
	udev = container_of(trig, struct uledtriggers_device, led_trigger);

	retval = mutex_lock_interruptible(&udev->mutex);
	if (retval)
		return retval;

	trig_state = udev->trig_state;
	switch (trig_state) {
	default:
	case TRIG_STATE_EVENT:
		led_set_brightness(led_cdev, udev->brightness);
		break;
	case TRIG_STATE_BLINK:
		delay_on = udev->trig_delay_on;
		delay_off = udev->trig_delay_off;
		led_blink_set(led_cdev, &delay_on, &delay_off);
		break;
	}
	mutex_unlock(&udev->mutex);
	return retval;
}

static int uledtriggers_open(struct inode *inode, struct file *file)
{
	struct uledtriggers_device *udev;

	udev = kzalloc(sizeof(*udev), GFP_KERNEL);
	if (!udev)
		return -ENOMEM;

	mutex_init(&udev->mutex);
	udev->state = ULEDTRIGGERS_STATE_UNKNOWN;

	file->private_data = udev;
	stream_open(inode, file);

	return 0;
}

/*
 * Name validation: Allow only alphanumeric, hyphen or underscore.
 */
static bool is_trigger_name_valid(const char * name)
{
	size_t i;

	if (name[0] == '\0')
		return false;

	for (i = 0; i < TRIG_NAME_MAX; i++) {
		if (name[i] == '\0')
			break;
		if (!isalnum(name[i]) && name[i] != '-' && name[i] != '_')
			return false;
	}
	/* Length check. */
	return (i < TRIG_NAME_MAX);
}

/*
 * Common setup code that can be called from either the write function or the
 * ioctl ULEDTRIGGERS_IOC_DEV_SETUP.
 */
static int dev_setup(struct uledtriggers_device *udev, const char __user *buffer)
{
	int retval;

	retval = mutex_lock_interruptible(&udev->mutex);
	if (retval)
		return retval;

	if (udev->state == ULEDTRIGGERS_STATE_REGISTERED) {
		retval = -EBUSY;
		goto out;
	}

	if (copy_from_user(&udev->user_dev, buffer,
			   sizeof(struct uledtriggers_user_dev))) {
		retval = -EFAULT;
		goto out;
	}

	if (!is_trigger_name_valid(udev->user_dev.name)) {
		retval = -EINVAL;
		goto out;
	}

	udev->led_trigger.name = udev->user_dev.name;
	udev->led_trigger.activate = uledtriggers_trig_activate;
	retval = led_trigger_register(&udev->led_trigger);
	if (retval < 0) {
		udev->led_trigger.name = NULL;
		goto out;
	}

	udev->state = ULEDTRIGGERS_STATE_REGISTERED;

out:
	mutex_unlock(&udev->mutex);

	return retval;
}

/*
 * Common code to set brightness.
 * It's called via write_user_buf_brightness() for the case of a brightness
 * value in a userspace buffer (write function or ioctl ULEDTRIGGERS_IOC_EVENT).
 * It's called directly for ioctl ULEDTRIGGERS_IOC_OFF and ULEDTRIGGERS_IOC_ON.
 */
static int write_brightness(struct uledtriggers_device *udev, int brightness)
{
	int retval;

	retval = mutex_lock_interruptible(&udev->mutex);
	if (retval)
		return retval;

	if (udev->state != ULEDTRIGGERS_STATE_REGISTERED) {
		retval = -EINVAL;
		goto out;
	}

	udev->trig_delay_on = 0u;
	udev->trig_delay_off = 0u;
	udev->brightness = brightness;
	udev->trig_state = TRIG_STATE_EVENT;
	led_trigger_event(&udev->led_trigger, brightness);

out:
	mutex_unlock(&udev->mutex);

	return retval;
}

/*
 * Common code to set brightness from a value stored in a userspace buffer.
 * This can be called from either the write function or the
 * ioctl ULEDTRIGGERS_IOC_EVENT.
 */
static int write_user_buf_brightness(struct uledtriggers_device *udev, const char __user *buffer)
{
	int brightness;

	if (copy_from_user(&brightness, buffer, sizeof(brightness))) {
		return -EFAULT;
	}

	return write_brightness(udev, brightness);
}

static ssize_t uledtriggers_write(struct file *file, const char __user *buffer,
	size_t count, loff_t *ppos)
{
	struct uledtriggers_device *udev = file->private_data;
	int retval;

	if (count == 0)
		return 0;

	switch (udev->state) {
	case ULEDTRIGGERS_STATE_UNKNOWN:
		if (count != sizeof(struct uledtriggers_user_dev)) {
			return -EINVAL;
		}
		retval = dev_setup(udev, buffer);
		if (retval < 0)
			return retval;
		return count;
	case ULEDTRIGGERS_STATE_REGISTERED:
		if (count != sizeof(int)) {
			return -EINVAL;
		}
		retval = write_user_buf_brightness(udev, buffer);
		if (retval < 0)
			return retval;
		return count;
	default:
		return -EBADFD;
	}
}

static long uledtriggers_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct uledtriggers_device *udev = file->private_data;
	struct uledtriggers_blink blink;
	struct uledtriggers_blink_oneshot blink_oneshot;
	int retval = 0;

	switch (cmd) {
	case ULEDTRIGGERS_IOC_DEV_SETUP:
		retval = dev_setup(udev, (const char __user *)arg);
		break;

	case ULEDTRIGGERS_IOC_OFF:
		retval = write_brightness(udev, LED_OFF);
		break;

	case ULEDTRIGGERS_IOC_ON:
		retval = write_brightness(udev, LED_FULL);
		break;

	case ULEDTRIGGERS_IOC_EVENT:
		retval = write_user_buf_brightness(udev, (const char __user *)arg);
		break;

	case ULEDTRIGGERS_IOC_BLINK:
		retval = copy_from_user(&blink,
			(struct uledtriggers_blink __user *)arg,
			sizeof(blink));
		if (retval)
			return retval;
		retval = mutex_lock_interruptible(&udev->mutex);
		if (retval)
			return retval;
		if (udev->state != ULEDTRIGGERS_STATE_REGISTERED) {
			mutex_unlock(&udev->mutex);
			return -EINVAL;
		}
		udev->trig_delay_on = blink.delay_on;
		udev->trig_delay_off = blink.delay_off;
		udev->brightness = LED_FULL;
		udev->trig_state = TRIG_STATE_BLINK;
		led_trigger_blink(&udev->led_trigger, blink.delay_on, blink.delay_off);
		mutex_unlock(&udev->mutex);
		break;

	case ULEDTRIGGERS_IOC_BLINK_ONESHOT:
		retval = copy_from_user(&blink_oneshot,
			(struct uledtriggers_blink_oneshot __user *)arg,
			sizeof(blink_oneshot));
		if (retval)
			return retval;
		if (blink_oneshot.__unused)
			return -EINVAL;
		retval = mutex_lock_interruptible(&udev->mutex);
		if (retval)
			return retval;
		if (udev->state != ULEDTRIGGERS_STATE_REGISTERED) {
			mutex_unlock(&udev->mutex);
			return -EINVAL;
		}
		udev->trig_delay_on = 0u;
		udev->trig_delay_off = 0u;
		udev->brightness = blink_oneshot.invert ? LED_FULL : LED_OFF;
		udev->trig_state = TRIG_STATE_EVENT;
		led_trigger_blink_oneshot(&udev->led_trigger, blink_oneshot.delay_on, blink_oneshot.delay_off, blink_oneshot.invert);
		mutex_unlock(&udev->mutex);
		break;

	default:
		retval = -ENOIOCTLCMD;
		break;
	}

	return retval;
}

static int uledtriggers_release(struct inode *inode, struct file *file)
{
	struct uledtriggers_device *udev = file->private_data;

	if (udev->state == ULEDTRIGGERS_STATE_REGISTERED) {
		udev->state = ULEDTRIGGERS_STATE_UNKNOWN;
		led_trigger_unregister(&udev->led_trigger);
	}
	kfree(udev);

	return 0;
}

static const struct file_operations uledtriggers_fops = {
	.owner		= THIS_MODULE,
	.open		= uledtriggers_open,
	.release	= uledtriggers_release,
	.write		= uledtriggers_write,
	.unlocked_ioctl	= uledtriggers_ioctl,
};

static struct miscdevice uledtriggers_misc = {
	.fops		= &uledtriggers_fops,
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= ULEDTRIGGERS_NAME,
};

module_misc_device(uledtriggers_misc);

MODULE_AUTHOR("Craig McQueen <craig@mcqueen.au>");
MODULE_DESCRIPTION("Userspace LED triggers driver");
MODULE_LICENSE("GPL");
