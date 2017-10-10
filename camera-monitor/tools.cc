// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/camera-monitor/tools.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/threading/platform_thread.h"

namespace huddly_monitor {

bool LookForErrorBlocking(std::string error_key, FILE *pipe,
                          std::string *err_msg) {
  const uint8_t kBufLength = 100;
  char buffer[kBufLength];
  std::string logs = "";
  if (!pipe) {
    *err_msg = "Opening pipe failed";
    return false;
  }

  while (!feof(pipe)) {
    memset(buffer, 0, kBufLength);
    if (fgets(buffer, kBufLength, pipe) != nullptr)
      logs.append(buffer);
    if (logs.rfind(error_key) != std::string::npos) {
      return true;
    }
  }

  return false;
}

uint32_t GetGpioNum(uint8_t bus_num, uint8_t port_num) {
  const uint32_t kFrontLeft = 218;
  const uint32_t kFrontRight = 219;
  const uint32_t kBackDual = 209;
  if (bus_num == 1) {
    switch (port_num) {
    case 2:
      return kFrontLeft;
    case 3:
      return kFrontRight;
    case 5:
    case 6:
      return kBackDual;
    default:
      return 0;
    }
  }

  if (bus_num == 2) {
    switch (port_num) {
    case 1:
      return kFrontLeft;
    case 2:
      return kFrontRight;
    case 3:
    case 4:
      return kBackDual;
    default:
      return 0;
    }
  }

  return 0;
}

bool GetDevice(uint16_t vid, uint16_t pid, libusb_device **device,
               std::string *err_msg) {
  libusb_context *context;
  libusb_device **device_list;
  *device = nullptr;

  if (libusb_init(&context)) {  // On failure.
    *err_msg = "Failed to get context";
    return false;
  }

  int num_devices = 0;
  if ((num_devices = libusb_get_device_list(context, &device_list)) < 0) {
    *err_msg = "Failed to get USB devices";
    return false;
  }

  for (int i = 0; i < num_devices; i++) {
    libusb_device_descriptor descriptor;

    // If device descriptor can be acquired, and acquired
    // descriptor belongs to desired device, return true.
    if (!libusb_get_device_descriptor(device_list[i], &descriptor) &&
        vid == descriptor.idVendor && pid == descriptor.idProduct) {
      *device = device_list[i];
      libusb_ref_device(*device);
    }
  }

  libusb_free_device_list(device_list, 1);

  return *device;
}

bool HotplugDeviceAt(uint32_t gpio_num) {
  const uint32_t kHotplugSleepTimeMilliseconds = 200;
  std::string gpio_string = std::to_string(gpio_num);

  // Create fd for gpio power toggling.
  base::FilePath export_path("/sys/class/gpio/export");
  if (base::WriteFile(export_path, gpio_string.c_str(), gpio_string.length()) <
      0) {
    LOG(WARNING) << "Failed to export GPIO fd.";
    return false;
  }

  // Set direction of byte flow in gpio.
  std::string out_string = "out";
  base::FilePath direction_path("/sys/class/gpio/gpio" + gpio_string +
                                "/direction");
  if (base::WriteFile(direction_path, out_string.c_str(), out_string.length()) <
      0) {
    LOG(WARNING) << "Failed to set GPIO direction.";
    return false;
  }

  // Turn off power going to gpio.
  std::string toggle = "0";
  base::FilePath toggle_path("/sys/class/gpio/gpio" + gpio_string + "/value");
  if (base::WriteFile(toggle_path, toggle.c_str(), toggle.length()) < 0) {
    LOG(WARNING) << "Failed to cut GPIO power.";
    return false;
  }

  // Wait to ensure power cycle.
  base::PlatformThread::Sleep(
      base::TimeDelta::FromMilliseconds(kHotplugSleepTimeMilliseconds));

  // Bring back power going to gpio.
  toggle = "1";
  if (base::WriteFile(toggle_path, toggle.c_str(), toggle.length()) < 0) {
    LOG(WARNING) << "Failed to turn on GPIO.";
    return false;
  }

  return true;
}

}  // namespace huddly_monitor
