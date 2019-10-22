// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAMERA_MONITOR_ABSTRACT_MONITOR_H_
#define CAMERA_MONITOR_ABSTRACT_MONITOR_H_

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string>

#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"

namespace huddly_monitor {

class AbstractMonitor {
 public:
  AbstractMonitor(bool init_wait_val, uint32_t sleep_time);
  virtual ~AbstractMonitor();

  // Interface used by implementing classes to update monitoring condition.
  // Pause monitoring when !user_defined_cond, continue otherwise.
  void UpdateCondition(bool user_defined_cond);

  // Must be called by user after all initialization is done.
  bool StartMonitor();

  // Interface to log the camera information every |time_interval|.
  virtual void StartMetricsLog(const base::TimeDelta &start_delay,
                               const base::TimeDelta &time_interval) = 0;

 private:
  base::Lock monitor_lock_;
  base::ConditionVariable wait_condition_;
  base::Thread *monitor_thread_;
  bool condition_;  // Shared resource.
  bool terminate_;  // Shared resource.
  uint32_t sleep_time_milliseconds_;

  void MonitorThread();
  void TerminateMonitorThread();
  bool Monitor(std::string *err_msg);
  void DestroyMonitor();

  // Implementation specific. Called by monitor to do checks
  // between intervals of sleep_time_milliseconds_.
  virtual bool VitalsExist() = 0;

  // Implementation specific. Called by monitor when !VitalExists().
  virtual bool Respond() = 0;
};

}  // namespace huddly_monitor

#endif  // CAMERA_MONITOR_ABSTRACT_MONITOR_H_
