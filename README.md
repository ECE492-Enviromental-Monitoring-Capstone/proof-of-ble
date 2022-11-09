# proof-of-ble
Proof of Concept for BLE usage

Random notes!

1. You need to build the latest version of bluez and run it in experimental mode
2. You need to punch a hole in dbus so that it doesn't disallow own names!

To do number 2 you need to add a conf file to `/etc/dbus-1/system.d/`
use the following commands to do this:

```
sudo touch /etc/dbus-1/system.d/aemi.conf
sudo vi /etc/dbus-1/system.d/aemi.conf
go to insert mode (i)

paste in this
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <policy context="default">
    <allow own="edu.aemi"/>
    <allow send_destination="edu.aemi"/>
    <allow receive_sender="edu.aemi"/>
  </policy>
</busconfig>

escape insert mode (ESC)
and save :wq

You need to restart for it to take effect.
sudo reboot
```
