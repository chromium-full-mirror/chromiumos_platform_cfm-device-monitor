// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIMO_MONITOR_DISPLAYLINK_MONITOR_H_
#define MIMO_MONITOR_DISPLAYLINK_MONITOR_H_

#include <libusb-1.0/libusb.h>
#include <string>

namespace mimo_monitor {

class DisplaylinkMonitor {
 public:
  DisplaylinkMonitor();
  DisplaylinkMonitor(const DisplaylinkMonitor&) = delete;
  DisplaylinkMonitor& operator=(const DisplaylinkMonitor&) = delete;

  bool IsDisplaylink(libusb_device *device,
                     const libusb_device_descriptor &desc);
  bool ReadFrameBuffer(libusb_device *device);
  void CheckDLHealth(libusb_device *device);
  bool DisplayStatus();

 private:
  int Peek(libusb_device_handle *dev_handle, int value, int addr,
           unsigned char *data, int data_len);

  // Port to read Displaylink serial number.
  int serial_number_port_;
  // Displaylink chip version.
  int displaylink_type_;
  // Serial number.
  std::string serial_;

  // Is Displaylink chip alive.
  bool display_alive_;
};

}  // namespace mimo_monitor

#endif  // MIMO_MONITOR_DISPLAYLINK_MONITOR_H_
