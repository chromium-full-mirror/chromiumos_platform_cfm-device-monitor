// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIMO_MONITOR_MIMO_MONITOR_H_
#define MIMO_MONITOR_MIMO_MONITOR_H_

#include <brillo/daemons/daemon.h>

#include <libusb-1.0/libusb.h>
#include <memory>

#include "cfm-device-monitor/mimo-monitor/displaylink_monitor.h"
#include "cfm-device-monitor/mimo-monitor/sis_monitor.h"

namespace mimo_monitor {

class MimoMonitor : public brillo::Daemon {
 public:
  explicit MimoMonitor(libusb_context *ctx_);
  MimoMonitor(const MimoMonitor&) = delete;
  MimoMonitor& operator=(const MimoMonitor&) = delete;

  ~MimoMonitor() override;

  static std::unique_ptr<MimoMonitor> Create();

  SiSMonitor sis_monitor;
  DisplaylinkMonitor dl_monitor;
  void FindMimo();
  void CheckMimoHealth();
  void TaskScheduler();

 protected:
  void OnShutdown(int *return_code) override;

 private:
  bool mimo_found_;
  libusb_device *display_device_;
  libusb_device *touch_device_;
  libusb_context *ctx_;

  base::WeakPtr<MimoMonitor> GetWeakPtr();
  base::WeakPtrFactory<MimoMonitor> weak_factory_;
};

}  // namespace mimo_monitor

#endif  // MIMO_MONITOR_MIMO_MONITOR_H_
