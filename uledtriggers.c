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

struct uledtriggers_device {
	struct uledtriggers_user_dev	user_dev;
	struct led_classdev	led_cdev;
	struct mutex		mutex;
	enum uledtriggers_state	state;
	wait_queue_head_t	waitq;
	int			brightness;
	bool			new_data;
};

static struct miscdevice uledtriggers_misc;

static void uledtriggers_brightness_set(struct led_classdev *led_cdev,
				 enum led_brightness brightness)
{
	struct uledtriggers_device *udev = container_of(led_cdev, struct uledtriggers_device,
						 led_cdev);

	if (udev->brightness != brightness) {
		udev->brightness = brightness;
		udev->new_data = true;
		wake_up_interruptible(&udev->waitq);
	}
}

static int uledtriggers_open(struct inode *inode, struct file *file)
{
	struct uledtriggers_device *udev;

	udev = kzalloc(sizeof(*udev), GFP_KERNEL);
	if (!udev)
		return -ENOMEM;

	udev->led_cdev.name = udev->user_dev.name;
	udev->led_cdev.brightness_set = uledtriggers_brightness_set;

	mutex_init(&udev->mutex);
	init_waitqueue_head(&udev->waitq);
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

	if (udev->user_dev.max_brightness <= 0) {
		ret = -EINVAL;
		goto out;
	}
	udev->led_cdev.max_brightness = udev->user_dev.max_brightness;

	ret = devm_led_classdev_register(uledtriggers_misc.this_device,
					 &udev->led_cdev);
	if (ret < 0)
		goto out;

	udev->new_data = true;
	udev->state = ULEDTRIGGERS_STATE_REGISTERED;
	ret = count;

out:
	mutex_unlock(&udev->mutex);

	return ret;
}

static ssize_t uledtriggers_read(struct file *file, char __user *buffer, size_t count,
			  loff_t *ppos)
{
	struct uledtriggers_device *udev = file->private_data;
	ssize_t retval;

	if (count < sizeof(udev->brightness))
		return 0;

	do {
		retval = mutex_lock_interruptible(&udev->mutex);
		if (retval)
			return retval;

		if (udev->state != ULEDTRIGGERS_STATE_REGISTERED) {
			retval = -ENODEV;
		} else if (!udev->new_data && (file->f_flags & O_NONBLOCK)) {
			retval = -EAGAIN;
		} else if (udev->new_data) {
			retval = copy_to_user(buffer, &udev->brightness,
					      sizeof(udev->brightness));
			udev->new_data = false;
			retval = sizeof(udev->brightness);
		}

		mutex_unlock(&udev->mutex);

		if (retval)
			break;

		if (!(file->f_flags & O_NONBLOCK))
			retval = wait_event_interruptible(udev->waitq,
					udev->new_data ||
					udev->state != ULEDTRIGGERS_STATE_REGISTERED);
	} while (retval == 0);

	return retval;
}

static __poll_t uledtriggers_poll(struct file *file, poll_table *wait)
{
	struct uledtriggers_device *udev = file->private_data;

	poll_wait(file, &udev->waitq, wait);

	if (udev->new_data)
		return EPOLLIN | EPOLLRDNORM;

	return 0;
}

static int uledtriggers_release(struct inode *inode, struct file *file)
{
	struct uledtriggers_device *udev = file->private_data;

	if (udev->state == ULEDTRIGGERS_STATE_REGISTERED) {
		udev->state = ULEDTRIGGERS_STATE_UNKNOWN;
		devm_led_classdev_unregister(uledtriggers_misc.this_device,
					     &udev->led_cdev);
	}
	kfree(udev);

	return 0;
}

static const struct file_operations uledtriggers_fops = {
	.owner		= THIS_MODULE,
	.open		= uledtriggers_open,
	.release	= uledtriggers_release,
	.read		= uledtriggers_read,
	.write		= uledtriggers_write,
	.poll		= uledtriggers_poll,
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
