// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/mimo-monitor/mimo_monitor.h"

#include <base/bind.h>
#include <base/logging.h>
#include <brillo/message_loops/message_loop.h>
#include <brillo/syslog_logging.h>

#include <libusb-1.0/libusb.h>
#include <stdlib.h>
#include <memory>

#include "cfm-device-monitor/mimo-monitor/utils.h"

namespace {

const int kFindMimoSleepTimeMs = 60e3;
const int kResetSleepTimeMs = 10e3;
const int kPingIntervalMs = 60e3;
const int kTouchInterface = 0;

}  // namespace

namespace mimo_monitor {

std::unique_ptr<MimoMonitor> MimoMonitor::Create() {
  libusb_context* ctx = nullptr;
  int ret = libusb_init(&ctx);
  if (ret < 0) {
    LOG(WARNING) << "Init libusb context fail. error " << ret;
    return nullptr;
  }
  return std::unique_ptr<MimoMonitor>(new MimoMonitor(ctx));
}

MimoMonitor::MimoMonitor(libusb_context* ctx) : ctx_(ctx), weak_factory_(this) {
  display_device_ = nullptr;
  touch_device_ = nullptr;
  mimo_found_ = false;

  libusb_set_debug(ctx_, 3);
}

MimoMonitor::~MimoMonitor() {
  if (display_device_) {
    libusb_unref_device(display_device_);
  }
  if (touch_device_) {
    libusb_unref_device(touch_device_);
  }
  libusb_exit(ctx_);
}

base::WeakPtr<MimoMonitor> MimoMonitor::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void MimoMonitor::FindMimo() {
  libusb_device** raw_devs = nullptr;
  libusb_device_descriptor desc;
  bool dl_found = false;
  bool sis_found = false;
  int ret, device_cnt = 0;

  ssize_t cnt = libusb_get_device_list(ctx_, &raw_devs);

  ScopedUsbDevs devs(raw_devs);

  // There was an error in getting the devices on the USB bus.
  if (cnt < 0) {
    LOG(WARNING) << "Get device list fail, error: " << cnt;
    mimo_found_ = false;
    // Post a task instead of call TaskScheduler directly so current function
    // won't be blocked by it.
    base::MessageLoop::current()->task_runner()->PostTask(
        FROM_HERE, base::Bind(&MimoMonitor::TaskScheduler, GetWeakPtr()));
    return;
  }

  // Interate through all the devices
  VLOG(1) << "Searching for SiS and DisplayLink in device list..";

  // Interate through all the devices
  for (ssize_t i = 0; i < cnt; i++) {
    libusb_device* dev = devs.get()[i];
    // Get device descriptor.
    ret = libusb_get_device_descriptor(dev, &desc);
    if (ret < 0) {
      LOG(WARNING) << "Failed to get devcie descriptor, error: " << ret;
      continue;
    }

    // Currently don't support multiple Mimos. So only try to find the first
    // device.
    if (!dl_found) {
      if (dl_monitor.IsDisplaylink(dev, desc)) {
        device_cnt += 1;
        dl_found = true;
        display_device_ = libusb_ref_device(dev);
        VLOG(1) << "Displaylink found.";
      }
    }
    if (!sis_found) {
      if (sis_monitor.IsSiS(dev, desc)) {
        device_cnt += 1;
        sis_found = true;
        touch_device_ = libusb_ref_device(dev);
        VLOG(1) << "SiS found.";
      }
    }
  }

  switch (device_cnt) {
    case 0: {
      // Nothing found.
      VLOG(1) << "Didn't find Displaylink or SiSMonitor";
      break;
    }
    case 1: {
      // Only found part of Mimo.
      LOG(WARNING) << "Only found part of Mimo.";
      break;
    }
    case 2: {
      // Mimo found.
      VLOG(1) << "Mimo found.";
      mimo_found_ = true;
      break;
    }
    default: {
      // Invalid number.
      LOG(WARNING) << "Invalid conponents number " << device_cnt
                   << " some error happens.";
    }
  }

  base::MessageLoop::current()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&MimoMonitor::TaskScheduler, GetWeakPtr()));
}

void MimoMonitor::CheckMimoHealth() {
  // Check status of Displaylink.
  dl_monitor.CheckDLHealth(display_device_);

  // Check status of SiS.
  sis_monitor.CheckSiSHealth(touch_device_);

  base::MessageLoop::current()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&MimoMonitor::TaskScheduler, GetWeakPtr()));
}

void MimoMonitor::TaskScheduler() {
  // Didn't find Mimo.
  if (!mimo_found_) {
    base::MessageLoop::current()->task_runner()->PostDelayedTask(
        FROM_HERE, base::Bind(&MimoMonitor::FindMimo, GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kFindMimoSleepTimeMs));
    return;
  }

  // Both touch and display fail.
  if (!dl_monitor.DisplayStatus() && !sis_monitor.TouchStatus()) {
    LOG(WARNING) << "Both touch and display are down. Resetting Mimo.";
    ResetDevice(libusb_get_parent(display_device_));
    mimo_found_ = false;

    base::MessageLoop::current()->task_runner()->PostDelayedTask(
        FROM_HERE, base::Bind(&MimoMonitor::FindMimo, GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kResetSleepTimeMs));
    return;
  }

  // Touch fails.
  if (!sis_monitor.TouchStatus()) {
    LOG(WARNING) << "Trying to reset touch panel.";
    mimo_found_ = false;
    sis_monitor.ResetSiS(touch_device_, kTouchInterface);

    base::MessageLoop::current()->task_runner()->PostDelayedTask(
        FROM_HERE, base::Bind(&MimoMonitor::FindMimo, GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kResetSleepTimeMs));
    return;
  }

  // Display fails.
  if (!dl_monitor.DisplayStatus()) {
    LOG(WARNING) << "Trying to reset display.";
    mimo_found_ = false;
    ResetDevice(display_device_);

    base::MessageLoop::current()->task_runner()->PostDelayedTask(
        FROM_HERE, base::Bind(&MimoMonitor::FindMimo, GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kResetSleepTimeMs));
    return;
  }

  // Everything looks good.
  VLOG(1) << "Mimo is alive.";

  base::MessageLoop::current()->task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&MimoMonitor::CheckMimoHealth, GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kPingIntervalMs));
  return;
}

void MimoMonitor::OnShutdown(int* return_code) {
  VLOG(1) << "Shutdown MimoMonitor, return code: " << *return_code;

  if (!mimo_found_) {
    return;
  }

  // Make sure SiS interface is attached back on shutdown.
  libusb_device_handle* dev_handle_ = NULL;
  int ret = libusb_open(touch_device_, &dev_handle_);
  if (ret < 0) {
    // There was an error opening the handle. Device is not there.
    LOG(WARNING) << "Failed to check SiS status on shutdown, error " << ret;
    return;
  }
  ScopedUsbDevHandle dev_handle(dev_handle_);

  ret = libusb_kernel_driver_active(dev_handle.get(), kTouchInterface);
  if (ret < 0) {
    LOG(WARNING) << "Failed to get kernel driver status on shutdown, error "
                 << ret;
    return;
  }

  if (ret == 0) {
    ret = libusb_attach_kernel_driver(dev_handle.get(), kTouchInterface);
    if (ret < 0) {
      LOG(WARNING) << "Re-attach SiS interface fail on shutdown, error " << ret;
      return;
    }
  }
}

}  // namespace mimo_monitor
