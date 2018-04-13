// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/camera-monitor/mock_monitor.h"

#include <iostream>
#include <string>

namespace huddly_monitor {

MockMonitor::MockMonitor(bool init_wait_val, uint32_t sleep_time)
    : AbstractMonitor(init_wait_val, sleep_time),
      local_condition(init_wait_val) {}

MockMonitor::~MockMonitor() {}

bool MockMonitor::VitalsExist() {
  VLOG(1) << "Vitals value --> " << std::to_string(vitals_exist_custom);
  if (!vitals_exist_custom) {
    VLOG(1) << "EXPECT RESPONSE";
  } else {
    VLOG(1) << "EXPECT NO RESPONSE";
  }
  return vitals_exist_custom;
}

bool MockMonitor::Respond() {
  VLOG(1) << "MockMonitor detected no vitals";
  return true;
}

void MockMonitor::StartMocking() {
  if (!StartMonitor()) {
    LOG(ERROR) << "Could not start mock monitor";
  }
  VLOG(1) << "Started mocking";
  uint64_t count = 0;
  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    count++;
    if (count % 10 == 0) {
      local_condition = !local_condition;
      UpdateCondition(local_condition, ++local_timestamp);
      VLOG(1) << "Updated condition to --> " << std::to_string(local_condition);
    } else if (count % 5 == 0) {
      vitals_exist_custom = !vitals_exist_custom;
      VLOG(1) << "Updated vitals value to --> "
              << std::to_string(vitals_exist_custom);
    }
  }
}

}  // namespace huddly_monitor
