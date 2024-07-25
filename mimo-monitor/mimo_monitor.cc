// Copyright 2017 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/mimo-monitor/mimo_monitor.h"

#include <base/functional/bind.h>
#include <base/logging.h>
#include <base/task/single_thread_task_runner.h>
#include <base/time/time.h>
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
const int64_t kResetThrottleMs = 10 * 60e3;  // 10 minutes
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

#if defined(LIBUSB_API_VERSION) && (LIBUSB_API_VERSION >= 0x01000106)
  libusb_set_option(ctx_, LIBUSB_OPTION_LOG_LEVEL, 3);
#else
  libusb_set_debug(ctx_, 3);
#endif
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
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&MimoMonitor::TaskScheduler, GetWeakPtr()));
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

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&MimoMonitor::TaskScheduler, GetWeakPtr()));
}

void MimoMonitor::CheckMimoHealth() {
  // Check status of Displaylink.
  dl_monitor.CheckDLHealth(display_device_);

  // Check status of SiS.
  sis_monitor.CheckSiSHealth(touch_device_);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&MimoMonitor::TaskScheduler, GetWeakPtr()));
}

void MimoMonitor::FindMimoAfter(int delayMs) {
  if (delayMs < 0) {
    delayMs = kResetSleepTimeMs;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&MimoMonitor::FindMimo, GetWeakPtr()),
      base::Milliseconds(delayMs));
}

bool MimoMonitor::AllowReset(base::Time now) {
  // Compare current time with oldest reset time. If delta exceeds 10 minutes,
  // save the instant time and return true (allow reset).  Otherwise false.
  const int next_reset_idx = (last_reset_idx_ + 1) % last_reset_ms_.size();
  const int64_t earliest_reset_ms = last_reset_ms_[next_reset_idx];
  const int64_t now_ms = now.InMillisecondsSinceUnixEpoch();
  if (now_ms - earliest_reset_ms > kResetThrottleMs) {
    last_reset_idx_ = next_reset_idx;
    last_reset_ms_[last_reset_idx_] = now_ms;
    return true;
  }
  return false;
}

void MimoMonitor::ResetParent() {
  mimo_found_ = false;
  if (AllowReset(base::Time::Now())) {
    LOG(WARNING) << "Resetting entire device.";
    ResetDevice(libusb_get_parent(display_device_));
  } else {
    VLOG(1) << "Not resetting entire device: throttled.";
  }
}

void MimoMonitor::ResetTouch() {
  mimo_found_ = false;
  const int ret = sis_monitor.ResetSiS(touch_device_, kTouchInterface);
  if (ret < 0) {
    ResetParent();
  }
}

void MimoMonitor::TaskScheduler() {
  // Didn't find Mimo.
  if (!mimo_found_) {
    FindMimoAfter(kFindMimoSleepTimeMs);
    return;
  }

  // Both touch and display fail.
  if (!dl_monitor.DisplayStatus() && !sis_monitor.TouchStatus()) {
    // Please see b/109866345 before modyfing the below line.
    LOG(WARNING) << "Both touch and display are down. Resetting Mimo.";
    ResetParent();
    FindMimoAfter();
    return;
  }

  // Touch fails.
  if (!sis_monitor.TouchStatus()) {
    // Please see b/109866345 before modyfing the below line.
    LOG(WARNING) << "Trying to reset touch panel.";
    ResetTouch();
    FindMimoAfter();
    return;
  }

  // Display fails.
  if (!dl_monitor.DisplayStatus()) {
    // Please see b/109866345 before modyfing the below line.
    LOG(WARNING) << "Trying to reset display.";
    mimo_found_ = false;
    ResetDevice(display_device_);
    FindMimoAfter();
    return;
  }

  // Everything looks good.
  VLOG(1) << "Mimo is alive.";

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&MimoMonitor::CheckMimoHealth, GetWeakPtr()),
      base::Milliseconds(kPingIntervalMs));
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
