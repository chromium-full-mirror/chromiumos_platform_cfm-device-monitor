// Copyright 2017 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIMO_MONITOR_MIMO_MONITOR_H_
#define MIMO_MONITOR_MIMO_MONITOR_H_

#include <base/time/time.h>
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
  void FindMimoAfter(int delayMs = -1);
  void ResetParent();
  void ResetTouch();
  void TaskScheduler();

 protected:
  void OnShutdown(int *return_code) override;

 private:
  bool mimo_found_;
  // Track the time of the last three touch panel resets and skip if under
  // 10 minutes.
  bool AllowReset(base::Time now);
  std::array<int64_t, 3> last_reset_ms_ = {0};
  int last_reset_idx_ = 0;
  FRIEND_TEST(MimoMonitorTest, TestResetThrottle);

  libusb_device *display_device_;
  libusb_device *touch_device_;
  libusb_context *ctx_;

  base::WeakPtr<MimoMonitor> GetWeakPtr();
  base::WeakPtrFactory<MimoMonitor> weak_factory_;
};

}  // namespace mimo_monitor

#endif  // MIMO_MONITOR_MIMO_MONITOR_H_
