// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAMERA_MONITOR_TOOLS_H_
#define CAMERA_MONITOR_TOOLS_H_

#include <libusb-1.0/libusb.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string>

namespace huddly_monitor {

// Reads FILE pointed to by file and searches for error_key. Blocks
// until an error is found or underlying infrastructure failure.
bool LookForErrorBlocking(std::string error_key, FILE *file,
                          std::string *err_msg);

// Guado specific.
uint32_t GetGpioNum(uint8_t bus_num, uint8_t port_num);

// Returns true and saves device pointer to *device.
// False and *device == nullptr on failure or device not found.
bool GetDevice(uint16_t vid, uint16_t pid, libusb_device **device,
               std::string *err_msg);

bool HotplugDeviceAt(uint32_t gpio_num);

}  // namespace huddly_monitor

#endif  // CAMERA_MONITOR_TOOLS_H_
