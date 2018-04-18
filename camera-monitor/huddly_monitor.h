// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAMERA_MONITOR_HUDDLY_MONITOR_H_
#define CAMERA_MONITOR_HUDDLY_MONITOR_H_

#include <libusb-1.0/libusb.h>
#include <pcrecpp.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include <base/macros.h>

#include "cfm-device-monitor/camera-monitor/abstract_monitor.h"

namespace huddly_monitor {

class HuddlyMonitor : public AbstractMonitor {
 public:
  HuddlyMonitor(bool init_wait_val, uint32_t sleep_time);
  ~HuddlyMonitor();

  bool VitalsExist() override;
  bool Respond() override;

 private:
  FILE *klog_pipe_;
  const std::string error_matcher_;
  const std::string error_exception_;

  DISALLOW_COPY_AND_ASSIGN(HuddlyMonitor);
};

}  // namespace huddly_monitor

#endif  // CAMERA_MONITOR_HUDDLY_MONITOR_H_
