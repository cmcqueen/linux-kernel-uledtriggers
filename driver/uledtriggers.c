// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Userspace LED triggers driver
 *
 * Copyright (C) 2025 Craig McQueen <craig.mcqueen@innerrange.com>
 */
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <uapi/linux/uledtriggers.h>

#define ULEDTRIGGERS_NAME	"uledtriggers"

enum uledtriggers_state {
	ULEDTRIGGERS_STATE_UNKNOWN,
	ULEDTRIGGERS_STATE_REGISTERED,
};

enum uledtriggers_trig_state {
	TRIG_STATE_OFF,
	TRIG_STATE_ON,
	TRIG_STATE_BLINK,
};

struct uledtriggers_device {
	struct uledtriggers_user_dev	user_dev;
	struct led_trigger	led_trigger;
	struct mutex		mutex;
	enum uledtriggers_state	state;
	enum uledtriggers_trig_state	trig_state;
	unsigned long		trig_delay_on;
	unsigned long		trig_delay_off;
};

static struct miscdevice uledtriggers_misc;

static char * trig_state_name(enum uledtriggers_trig_state trig_state)
{
	switch (trig_state) {
		case TRIG_STATE_OFF:	return "off";
		case TRIG_STATE_ON:	return "on";
		case TRIG_STATE_BLINK:	return "blink";
		default:		return "unknown";
	}
	return "unknown";
}

static void set_led_trigger(struct uledtriggers_device *data)
{
	dev_dbg(uledtriggers_misc.this_device, "LED trigger %s set to %s\n",
			data->led_trigger.name,
			trig_state_name(data->trig_state));
	switch (data->trig_state) {
	default:
	case TRIG_STATE_OFF:
		led_trigger_event(&data->led_trigger, LED_OFF);
		break;
	case TRIG_STATE_ON:
		led_trigger_event(&data->led_trigger, LED_FULL);
		break;
	case TRIG_STATE_BLINK:
		led_trigger_blink(&data->led_trigger, data->trig_delay_on, data->trig_delay_off);
		break;
	}
}

/*
 * When an LED is connected to the trigger, this 'activate' function runs and
 * sets the initial state of the LED.
 */
static int uledtriggers_trig_activate(struct led_classdev *led_cdev)
{
	struct led_trigger		*trig;
	struct uledtriggers_device	*data;

	trig = led_cdev->trigger;
	data = container_of(trig, struct uledtriggers_device, led_trigger);
	set_led_trigger(data);
	return 0;
}

static int uledtriggers_open(struct inode *inode, struct file *file)
{
	struct uledtriggers_device *udev;

	udev = kzalloc(sizeof(*udev), GFP_KERNEL);
	if (!udev)
		return -ENOMEM;

	udev->led_trigger.name = udev->user_dev.name;
	udev->led_trigger.activate = uledtriggers_trig_activate;

	mutex_init(&udev->mutex);
	udev->state = ULEDTRIGGERS_STATE_UNKNOWN;

	file->private_data = udev;
	stream_open(inode, file);

	return 0;
}

static ssize_t uledtriggers_write(struct file *file, const char __user *buffer,
			   size_t count, loff_t *ppos)
{
	struct uledtriggers_device *udev = file->private_data;
	const char *name;
	int ret;

	if (count == 0)
		return 0;

	ret = mutex_lock_interruptible(&udev->mutex);
	if (ret)
		return ret;

	if (udev->state == ULEDTRIGGERS_STATE_REGISTERED) {
		ret = -EBUSY;
		goto out;
	}

	if (count != sizeof(struct uledtriggers_user_dev)) {
		ret = -EINVAL;
		goto out;
	}

	if (copy_from_user(&udev->user_dev, buffer,
			   sizeof(struct uledtriggers_user_dev))) {
		ret = -EFAULT;
		goto out;
	}

	name = udev->user_dev.name;
	if (!name[0] || !strcmp(name, ".") || !strcmp(name, "..") ||
	    strchr(name, '/')) {
		ret = -EINVAL;
		goto out;
	}

	ret = led_trigger_register(&udev->led_trigger);
	if (ret < 0)
		goto out;

	udev->state = ULEDTRIGGERS_STATE_REGISTERED;
	ret = count;

out:
	mutex_unlock(&udev->mutex);

	return ret;
}

static long uledtriggers_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct uledtriggers_device *udev = file->private_data;
	struct uledtriggers_blink blink;
	struct uledtriggers_blink_oneshot blink_oneshot;
	int retval = 0;

	/*
	 * the direction is a bitmask, and VERIFY_WRITE catches R/W
	 * transfers. `Direction' is user-oriented, while
	 * access_ok is kernel-oriented, so the concept of "read" and
	 * "write" is reversed
	 */
	retval = 0;
	if (_IOC_DIR(cmd) & _IOC_READ)
		retval = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		retval =  !access_ok((void __user *)arg, _IOC_SIZE(cmd));
	if (retval)
		return -EFAULT;

	switch (cmd) {
	case ULEDTRIGGERS_IOC_OFF:
		udev->trig_delay_on = 0u;
		udev->trig_delay_off = 0u;
		udev->trig_state = TRIG_STATE_OFF;
		led_trigger_event(&udev->led_trigger, LED_OFF);
		break;

	case ULEDTRIGGERS_IOC_ON:
		udev->trig_delay_on = 0u;
		udev->trig_delay_off = 0u;
		udev->trig_state = TRIG_STATE_ON;
		led_trigger_event(&udev->led_trigger, LED_FULL);
		break;

	case ULEDTRIGGERS_IOC_BLINK:
		retval = copy_from_user(&blink,
			(struct uledtriggers_blink __user *)arg,
			sizeof(blink));
		if (retval)
			return retval;
		udev->trig_delay_on = blink.delay_on;
		udev->trig_delay_off = blink.delay_off;
		udev->trig_state = TRIG_STATE_BLINK;
		led_trigger_blink(&udev->led_trigger, blink.delay_on, blink.delay_off);
		break;

	case ULEDTRIGGERS_IOC_BLINK_ONESHOT:
		retval = copy_from_user(&blink_oneshot,
			(struct uledtriggers_blink_oneshot __user *)arg,
			sizeof(blink_oneshot));
		if (retval)
			return retval;
		udev->trig_delay_on = 0u;
		udev->trig_delay_off = 0u;
		udev->trig_state = blink_oneshot.invert ? TRIG_STATE_ON : TRIG_STATE_OFF;
		led_trigger_blink_oneshot(&udev->led_trigger, blink_oneshot.delay_on, blink_oneshot.delay_off, blink_oneshot.invert);
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
	.llseek		= no_llseek,
};

static struct miscdevice uledtriggers_misc = {
	.fops		= &uledtriggers_fops,
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= ULEDTRIGGERS_NAME,
};

module_misc_device(uledtriggers_misc);

MODULE_AUTHOR("Craig McQueen <craig.mcqueen@innerrange.com>");
MODULE_DESCRIPTION("Userspace LED triggers driver");
MODULE_LICENSE("GPL");
