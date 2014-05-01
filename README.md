PoC ASUS Xtion kernel driver
============================

I wanted to have a low-overhead video4linux driver for use with the
ASUS Xtion PRO LIVE structured light camera.

Requirements:

* An Xtion device with up-to-date firmware (has been tested with v8.5.34).
  Firmware update is available from ASUS support site.

After you load the driver, it creates two video devices for your camera
(color and depth). You can use udev rules to name them sensibly:

<pre>
# Rules for the xtion kernel driver
SUBSYSTEM=="video4linux", ATTR{xtion_endpoint}=="depth", ATTRS{xtion_id}=="*", SYMLINK+="xtion_$attr{xtion_id}_depth"
SUBSYSTEM=="video4linux", ATTR{xtion_endpoint}=="color", ATTRS{xtion_id}=="*", SYMLINK+="xtion_$attr{xtion_id}_color"
</pre>
