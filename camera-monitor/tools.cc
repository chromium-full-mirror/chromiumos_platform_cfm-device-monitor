// Copyright 2017 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/camera-monitor/tools.h"

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/threading/platform_thread.h>
#include <base/time/time.h>
#include <poll.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace huddly_monitor {

namespace {

const base::TimeDelta kHotplugSleepTime =
    base::Milliseconds(200);
const uint8_t kBufLength = 100;

}  // namespace

void ConsumeAndDiscardInput(FILE *stream) {
  char buffer[kBufLength];
  int fd = fileno(stream);

  base::TimeTicks start_time = base::TimeTicks::Now();
  while (base::TimeTicks::Now() - start_time < kHotplugSleepTime) {
    struct pollfd fds = { fd, POLLIN, 0 };

    if (poll(&fds, 1, 100) && (fds.revents & POLLIN)) {
      HANDLE_EINTR(read(fd, buffer, kBufLength));
    }
  }
}

bool LookForErrorBlocking(const std::string &error_key,
                          const std::string &error_exception,
                          FILE *pipe,
                          std::string *err_msg) {
  char buffer[kBufLength];
  std::string logs = "";
  if (!pipe) {
    *err_msg = "Opening pipe failed";
    return false;
  }
  int fd = fileno(pipe);

  while (true) {
    memset(buffer, 0, kBufLength);
    // Use read so that fetched lines are not buffered. Newlines will not be
    // handled properly after a match but this should be OK as the rest of the
    // stream is consumed and discarded when this happens.
    ssize_t read_result = HANDLE_EINTR(read(fd, buffer, kBufLength));

    if (read_result > 0)
      logs.append(buffer, 0, read_result);
    else
      break;

    if (logs.rfind(error_key) != std::string::npos) {
      // TODO(felixe): Remove risk of matching |error_key| but not
      // |error_exception| if read() breaks the line at an unlucky place.
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

  return *device != nullptr;
}

bool HotplugDeviceGuado(uint16_t vid, uint16_t pid, FILE* stream) {
  // Guado uses /sys/class/gpio/* to control VBUS power to the USB ports.
  libusb_device *huddly = nullptr;
  std::string err_msg = "";
  if (!GetDevice(vid, pid, &huddly, &err_msg)) {
    LOG(INFO) << "Failed to find camera. Cannot hotplug.";
    if (!err_msg.empty()) LOG(ERROR) << err_msg;
    return false;
  }
  uint8_t bus_num = libusb_get_bus_number(huddly);
  uint8_t port_num = libusb_get_port_number(huddly);
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
  if (!base::WriteFile(direction_path, out_string)) {
    LOG(WARNING) << "Failed to set GPIO direction.";
    return false;
  }

  // Turn off power going to gpio.
  std::string toggle = "0";
  base::FilePath toggle_path("/sys/class/gpio/gpio" + gpio_string + "/value");
  if (!base::WriteFile(toggle_path, toggle)) {
    LOG(WARNING) << "Failed to cut GPIO power.";
    return false;
  }

  // Consume and discard all further input to avoid triggering restarts for
  // things that happened before the power down.
  ConsumeAndDiscardInput(stream);

  // Bring back power going to gpio.
  toggle = "1";
  if (!base::WriteFile(toggle_path, toggle)) {
    LOG(WARNING) << "Failed to turn on GPIO.";
    return false;
  }

  return true;
}

}  // namespace huddly_monitor
