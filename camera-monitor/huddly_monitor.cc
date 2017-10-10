// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/camera-monitor/huddly_monitor.h"

#include <fstream>
#include <sstream>
#include <string>

#include "cfm-device-monitor/camera-monitor/tools.h"
#include "base/logging.h"
#include "base/threading/platform_thread.h"

namespace huddly_monitor {
constexpr uint16_t kHuddlyVid = 0x2bd9;
constexpr uint16_t kHuddlyPid = 0x0011;

HuddlyMonitor::HuddlyMonitor(bool init_wait_val, uint32_t sleep_time)
    : AbstractMonitor(init_wait_val, sleep_time),
      klog_pipe_(popen("dmesg -w --level=err", "r")) {}

HuddlyMonitor::~HuddlyMonitor() { pclose(klog_pipe_); }

bool HuddlyMonitor::VitalsExist() {
  std::string msg = "";
  std::string error_key = "uvcvideo: Failed";

  bool found_error = LookForErrorBlocking(error_key, klog_pipe_, &msg);

  if (!msg.empty()) {
    LOG(ERROR) << "Failed trying to monitor camera. " << msg;
  }

  return !found_error;
}

bool HuddlyMonitor::Respond() {
  std::string err_msg = "";
  const uint32_t kRebootSleepTimeSeconds = 30;

  libusb_device *huddly = nullptr;
  if (!GetDevice(kHuddlyVid, kHuddlyPid, &huddly, &err_msg)) {
    LOG(INFO) << "Failed to find camera. Cannot hotplug.";
    if (!err_msg.empty())
      LOG(ERROR) << err_msg;
    return false;
  }

  uint8_t bus_num = libusb_get_bus_number(huddly);
  uint8_t port_num = libusb_get_port_number(huddly);
  uint32_t gpio_num = GetGpioNum(bus_num, port_num);

  if (!gpio_num) {  // unknown gpio number.
    LOG(ERROR) << "Failed to get gpio number.";
    return false;
  }

  if (!HotplugDeviceAt(gpio_num)) {
    LOG(WARNING) << "Failed to hotplug.";
    return false;
  }

  LOG(WARNING) << "Detected crashed camera. Rebooted.";
  base::PlatformThread::Sleep(
      base::TimeDelta::FromSeconds(kRebootSleepTimeSeconds));
  return true;
}
}  // namespace huddly_monitor
