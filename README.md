# CFM Device Monitor

Source code for CFM peripheral monitors which check the status of CFM devices
and guarantee their liveness.

## mimo-monitor

The MIMO touch panel is a USB 2.0 device that contains a 1280x800 display and a
touch panel.

```
# Example lsusb output

Bus 001 Device 006: ID 17e9:416d DisplayLink MIMO VUE HD
Bus 001 Device 005: ID 266e:0110 Silicon Integrated Systems SiS HID Touch Controller

/:  Bus 01.Port 1: Dev 1, Class=root_hub, Driver=xhci_hcd/12p, 480M
    |__ Port 2: Dev 3, If 0, Class=Hub, Driver=hub/4p, 480M
        |__ Port 1: Dev 5, If 0, Class=Human Interface Device, Driver=usbhid, 12M
        |__ Port 3: Dev 6, If 0, Class=Vendor Specific Class, Driver=udl, 480M
        |__ Port 3: Dev 6, If 1, Class=Human Interface Device, Driver=usbhid, 480M
```

Each minute, the mimo-monitor checks that both logical USB devices are present,
and attempts to reset them if not.

### sis_monitor

SiS Monitor resets are 6-byte messages sent to the /dev/hidraw# endpoint. This
is like an application-level disable/enable.

If you `echo '1-2.1' > /sys/bus/usb/drivers/usb/unbind`, then the device will
disappear from `lsusb -t` but still be visible on `lsusb`. This is a lower-level
unbind, but not to the level of re-enumeration.
