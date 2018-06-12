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

constexpr uint16_t kHuddlyVid = 0x2bd9;
constexpr uint16_t kHuddlyPid = 0x0011;

void ConsumeAndDiscardInput(FILE *stream);

// Reads FILE pointed to by file and searches for error_key. Blocks until an
// error is found or underlying infrastructure failure. If |error_exception| is
// non-empty then only errors matching |error_key| and not |error_exception| are
// returned.
bool LookForErrorBlocking(const std::string& error_key,
                          const std::string& error_exception,
                          FILE *file,
                          std::string *err_msg);

// Guado specific.
uint32_t GetGpioNumGuado(uint8_t bus_num, uint8_t port_num);

// Returns true and saves device pointer to *device.
// False and *device == nullptr on failure or device not found.
bool GetDevice(uint16_t vid, uint16_t pid,
               libusb_device **device,
               std::string *err_msg);

// TODO(felixe): Restructure the code to put the file reading logic outside the
// hotplug actuator.
// Hotplug device using GPIO and while the device is powered off, consume and
// discard any data on the stream in a non-blocking fashion.
bool HotplugDeviceGuado(uint16_t vid, uint16_t pid, FILE *stream);

}  // namespace huddly_monitor

#endif  // CAMERA_MONITOR_TOOLS_H_
