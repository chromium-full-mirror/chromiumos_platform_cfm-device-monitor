// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIMO_MONITOR_UTILS_H_
#define MIMO_MONITOR_UTILS_H_

#include <time.h>

#include <libusb-1.0/libusb.h>
#include <string>

#include "base/scoped_generic.h"

namespace mimo_monitor {

int ReadID(libusb_device *device, int port, std::string *result);
struct tm GetLocalTime();
bool UseDeepPing(bool pre_condition);
bool ResetDevice(libusb_device *device);

struct ScopedDevHandleTraits {
  static libusb_device_handle *InvalidValue() { return nullptr; }
  static void Free(libusb_device_handle *dev_handle) {
    libusb_close(dev_handle);
  }
};

typedef base::ScopedGeneric<libusb_device_handle *, ScopedDevHandleTraits>
    ScopedUsbDevHandle;

struct ScopedCfgDescTraits {
  static libusb_config_descriptor *InvalidValue() { return nullptr; }
  static void Free(libusb_config_descriptor *cfg_desc) {
    libusb_free_config_descriptor(cfg_desc);
  }
};

typedef base::ScopedGeneric<libusb_config_descriptor *, ScopedCfgDescTraits>
    ScopedUsbCfgDesc;

struct ScopedUsbDevsTraits {
  static libusb_device **InvalidValue() { return nullptr; }
  static void Free(libusb_device **devs) { libusb_free_device_list(devs, 1); }
};

typedef base::ScopedGeneric<libusb_device **, ScopedUsbDevsTraits>
    ScopedUsbDevs;

}  // namespace mimo_monitor

#endif  // MIMO_MONITOR_UTILS_H_
