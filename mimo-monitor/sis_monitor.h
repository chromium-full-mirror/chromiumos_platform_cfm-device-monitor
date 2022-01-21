// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIMO_MONITOR_SIS_MONITOR_H_
#define MIMO_MONITOR_SIS_MONITOR_H_

#include <libusb-1.0/libusb.h>
#include <string>

namespace mimo_monitor {

class SiSMonitor {
 public:
  SiSMonitor();
  SiSMonitor(const SiSMonitor&) = delete;
  SiSMonitor& operator=(const SiSMonitor&) = delete;

  bool IsSiS(libusb_device *device, const libusb_device_descriptor &desc);
  bool PingSiS(libusb_device *device, int interface);
  int ResetSiS(libusb_device *device, int interface);
  void CheckSiSHealth(libusb_device *device);
  bool TouchStatus();

 private:
  int SendResetCmd(libusb_device_handle *dev_handle, int interface);
  int SendSiSCommand(libusb_device_handle *dev_handle, const unsigned char *cmd,
                     int cmd_size, unsigned char *response,
                     int max_response_size);
  bool CheckPingResponse(unsigned char *response, int size);
  bool CheckResetResponse(unsigned char *response, int size);

  // For touch_version, 1 = SiS9255, 2 = SiS9225 + F321, 0 = Everything else.
  enum touch_versions_ { SiS9255 = 1, SiS9255_F321 = 2, Unknown = 0 };
  touch_versions_ touch_version_;
  bool touch_status_;
  int mfg_size_;
  int mfg_port_;
  int addr_in_;
  int addr_out_;
  std::string manufacturer_;
};

}  // namespace mimo_monitor

#endif  // MIMO_MONITOR_SIS_MONITOR_H_
