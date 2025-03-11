======================
Userspace LED Triggers
======================

The uledtriggers driver supports userspace LED triggers. This can be useful
to create a more flexible architecture for applications to control LEDs.


Usage
=====

When the driver is loaded, a character device is created at /dev/uledtriggers.
To create a new LED trigger, open /dev/uledtriggers and write a
uledtriggers_user_dev structure to it (found in kernel public header file
linux/uledtriggers.h)::

    #define LED_TRIGGER_MAX_NAME_SIZE 64

    struct uledtriggers_user_dev {
	char name[LED_TRIGGER_MAX_NAME_SIZE];
    };

A new LED trigger will be created with the name given. The name can consist of
alphanumeric, hyphen and underscore characters.

After the initial setup, writing an int value will set the trigger's
brightness, equivalent to calling led_trigger_event().

Alternatively, there are ioctls (defined in the public header file) for setup,
changing trigger brightness, or doing blinking.

The LED trigger will be removed when the open file handle to /dev/uledtriggers
is closed.

Multiple LED triggers are created by opening additional file handles to
/dev/uledtriggers.
