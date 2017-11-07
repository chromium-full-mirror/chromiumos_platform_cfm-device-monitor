// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/mimo-monitor/utils.h"

#include <time.h>

#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <brillo/syslog_logging.h>
#include <libusb-1.0/libusb.h>
#include <string>

using base::StringPrintf;

namespace mimo_monitor {

int ReadID(libusb_device* device, int port, std::string* result) {
  int ret = 0;
  libusb_device_handle* raw_dev_handle = nullptr;
  unsigned char buf[128];

  ret = libusb_open(device, &raw_dev_handle);
  if (ret < 0) {
    // There was an error opening the handle. Device is not there, or handle is
    // invalid.
    LOG(WARNING) << "Open deivce fail at port " << port
                 << " in ReadID, error: " << ret;
    return -1;
  }

  ScopedUsbDevHandle dev_handle(raw_dev_handle);

  // Reads USB descriptior in text.
  ret = libusb_get_string_descriptor_ascii(dev_handle.get(), port, buf,
                                           sizeof(buf));
  if (ret < 0) {
    LOG(WARNING) << "Read device descriptor fail, error: " << ret;
    return -1;
  }

  if (result) {
    *result = std::string(reinterpret_cast<char*>(buf), size_t(ret));
  }

  return ret;
}

struct tm GetLocalTime() {
  time_t t = time(0);
  struct tm local_time;
  localtime_r(&t, &local_time);
  return local_time;
}

bool UseDeepPing(bool pre_condition) {
  if (!pre_condition) {
    return false;
  }
  struct tm time = GetLocalTime();
  if ((time.tm_hour == 0 && time.tm_min < 2) ||
      (time.tm_hour == 5 && time.tm_min < 2)) {
    return true;
  }
  return false;
}

bool ResetDevice(libusb_device* device) {
  libusb_device_handle* raw_dev_handle = nullptr;
  int ret = 0;

  ret = libusb_open(device, &raw_dev_handle);
  if (ret < 0) {
    LOG(WARNING) << "Failed to open device in ResetDevice, error " << ret;
    return false;
  }

  ScopedUsbDevHandle dev_handle(raw_dev_handle);

  ret = libusb_reset_device(dev_handle.get());
  if (ret < 0) {
    LOG(WARNING) << "Failed to reset device, error " << ret;
    return false;
  }

  return true;
}

}  // namespace mimo_monitor
