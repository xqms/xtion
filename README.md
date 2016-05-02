PoC ASUS Xtion kernel driver
============================

I wanted to have a low-overhead video4linux driver for use with the
ASUS Xtion PRO LIVE structured light camera.

Requirements:

* An Xtion device with up-to-date firmware (has been tested with v5.8.22).
  Firmware update is available from ASUS support site.

Usage
-----

After you load the driver, it creates two video devices for your camera
(color and depth). You can use udev rules to name them sensibly:

<pre>
# Rules for the xtion kernel driver
SUBSYSTEM=="video4linux", ATTR{xtion_endpoint}=="depth", ATTRS{xtion_id}=="*", SYMLINK+="xtion_$attr{xtion_id}_depth"
SUBSYSTEM=="video4linux", ATTR{xtion_endpoint}=="color", ATTRS{xtion_id}=="*", SYMLINK+="xtion_$attr{xtion_id}_color"
</pre>

The color device can be used with almost any Linux application that uses
the Video4Linux interface, e.g. most webcam apps. I recommend [guvcview][],
since it allows you to play with the resolution/controls.

The depth device creates unsigned 16-bit depth images, which most webcam
applications cannot display.

A ROS nodelet driver for use with this kernel driver is also available:
[xtion_grabber][].

Known Issues
------------

* The `snd_usb_audio` module might claim the USB interface first. If you
  see messages like
  `xtion 1-4:1.0: xtion_probe: failed to claim interface  1 (-16)!`,
  you might want to blacklist the `snd_usb_audio` module.
  For details, see issue https://github.com/xqms/xtion/issues/15.

[guvcview]: http://sourceforge.net/projects/guvcview/
[xtion_grabber]: https://github.com/xqms/xtion_grabber/
