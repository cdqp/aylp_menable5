Anyloop plugin for the MicroEnable 5 frame grabber
==================================================

Linux only for now. Only tested on a Mikrotron camera.


libaylp dependency
------------------

Symlink or copy the `libaylp` directory from anyloop to `libaylp`. For example:

```sh
ln -s $HOME/git/anyloop/libaylp libaylp
```


basler-fgsdk5 dependency
------------------------

Download Basler's Framegrabber SDK from
<https://www.baslerweb.com/en/downloads/software-downloads/> and either install
the SDK or symlink or copy it into a directory named`libfgsdk`.

Then, when running `sdk_init` with this plugin, make sure you include
`libfgsdk/lib` in your `LD_LIBRARY_PATH`. You won't have to do this while
running anyloop.


Building
--------

Use meson:

```sh
meson setup build
meson compile -C build
```


Initializing
------------

For some reason, talking to the kernel driver using only ioctl interrupts is not
completely reliable. It seems that we need to initialize with the SDK once, and
after that it all our own code *should* work across reboots. This initialization
is probably happening over `/sys/class/menable/menable0/pci_dev/resource*` but
I've yet to write code to replay those messages.

As such, just run the `sdk_init` binary which I *think* takes care of
initialization until other code using the Basler SDK is run.

Note that the `sdk_init` binary determines the width and height of the acquired
image.

