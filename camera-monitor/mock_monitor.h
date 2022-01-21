// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAMERA_MONITOR_MOCK_MONITOR_H_
#define CAMERA_MONITOR_MOCK_MONITOR_H_

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#include "cfm-device-monitor/camera-monitor/abstract_monitor.h"

namespace huddly_monitor {

class MockMonitor : public AbstractMonitor {
 public:
  MockMonitor(bool init_wait_val, uint32_t sleep_time);
  MockMonitor(const MockMonitor&) = delete;
  MockMonitor& operator=(const MockMonitor&) = delete;

  ~MockMonitor();

 private:
  // AbstractMonitor
  bool VitalsExist() override;
  bool Respond() override;

  void StartMocking();

  // Local copies of state private to AbstractMonitor, representing the mocked
  // class.
  bool local_condition;
  bool vitals_exist_custom = true;
  uint64_t local_timestamp = 0;
};

}  // namespace huddly_monitor

#endif  // CAMERA_MONITOR_MOCK_MONITOR_H_
