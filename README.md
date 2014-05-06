PoC ASUS Xtion kernel driver
============================

I wanted to have a low-overhead video4linux driver for use with the
ASUS Xtion PRO LIVE structured light camera.

Requirements:

* An Xtion device with up-to-date firmware (has been tested with v8.5.34).
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

[guvcview]: http://sourceforge.net/projects/guvcview/
[xtion_grabber]: https://github.com/xqms/xtion_grabber/
