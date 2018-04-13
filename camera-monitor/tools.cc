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
#include "base/sys_info.h"
#include "base/threading/platform_thread.h"

namespace huddly_monitor {

bool LookForErrorBlocking(const std::string &error_key,
                          const std::string &error_exception,
                          FILE *pipe,
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
      return error_exception.empty() ||
             logs.rfind(error_exception) == std::string::npos;
    }
  }

  return false;
}

uint32_t GetGpioNumGuado(uint8_t bus_num, uint8_t port_num) {
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

uint8_t GetUsbPortIdFizz(uint8_t bus_num, uint8_t port_num) {
  if (bus_num != 1) {
    return UINT8_MAX;
  }
  switch (port_num) {
    case 2:
      return 2;
    case 3:
      return 3;
    case 4:
      return 4;
    case 5:
      return 0;
    case 6:
      return 1;
    default:
      return UINT8_MAX;
  }
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

bool HotplugDeviceAt(uint8_t bus_num, uint8_t port_num) {
  const uint32_t kHotplugSleepTimeMilliseconds = 200;

  std::string board = base::SysInfo::GetLsbReleaseBoard();
  if (board == "fizz") {
    // Fizz and its derivatives uses /sys/class/chromeos/cros_ec/usb/vbus_* to
    // control VBUS power to the USB ports.
    uint8_t port_id = GetUsbPortIdFizz(bus_num, port_num);
    const std::string port_id_string = std::to_string(port_id);
    const base::FilePath base_path("/sys/class/chromeos/cros_ec/usb");
    const base::FilePath disable_path = base_path.Append("vbus_disable");
    const base::FilePath enable_path = base_path.Append("vbus_enable");

    // Turn off power on USB port.
    if (base::WriteFile(disable_path, port_id_string.c_str(),
                        port_id_string.length()) < 0) {
      LOG(WARNING) << "Failed to disable power on USB port " << port_id_string;
      return false;
    }

    // Wait to ensure power cycle.
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMilliseconds(kHotplugSleepTimeMilliseconds));

    // Bring back power on USB port.
    if (base::WriteFile(enable_path, port_id_string.c_str(),
                        port_id_string.length()) < 0) {
      LOG(WARNING) << "Failed to enable power on USB port " << port_id_string;
      return false;
    }

    return true;
  } else if (board == "guado") {
    // Guado uses /sys/class/gpio/* to control VBUS power to the USB ports.
    uint32_t gpio_num = GetGpioNumGuado(bus_num, port_num);
    if (!gpio_num) {  // unknown gpio number.
      LOG(ERROR) << "Failed to get gpio number.";
      return false;
    }
    std::string gpio_string = std::to_string(gpio_num);

    // Set direction of byte flow in gpio.
    std::string out_string = "out";
    base::FilePath direction_path("/sys/class/gpio/gpio" + gpio_string +
                                  "/direction");
    if (base::WriteFile(direction_path, out_string.c_str(),
                        out_string.length()) < 0) {
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
  } else {
    LOG(ERROR) << "Tried to hotplug a device on an unknown board.";
    return false;
  }
}

}  // namespace huddly_monitor
