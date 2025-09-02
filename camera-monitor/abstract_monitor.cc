// Copyright 2017 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/camera-monitor/abstract_monitor.h"

#include <assert.h>
#include <base/functional/bind.h>
#include <base/logging.h>
#include <base/run_loop.h>
#include <base/task/single_thread_task_runner.h>
#include <base/time/time.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include <string>

namespace huddly_monitor {

AbstractMonitor::AbstractMonitor(bool init_wait_val, uint32_t sleep_time)
    : monitor_lock_(),
      wait_condition_(&monitor_lock_),
      monitor_thread_(new base::Thread("monitor_thread")),
      condition_(init_wait_val),
      terminate_(false),
      sleep_time_milliseconds_(sleep_time) {}

AbstractMonitor::~AbstractMonitor() { DestroyMonitor(); }

void AbstractMonitor::UpdateCondition(bool user_defined_cond) {
  monitor_lock_.Acquire();

  if ((condition_ = user_defined_cond)) {
    wait_condition_.Signal();
  }

  monitor_lock_.Release();
}

bool AbstractMonitor::StartMonitor() {
  monitor_thread_->Start();
  monitor_thread_->task_runner().get()->PostTask(
      FROM_HERE,
      base::BindOnce(&AbstractMonitor::MonitorThread, base::Unretained(this)));
  return true;
}

void AbstractMonitor::MonitorThread() {
  std::string err_msg;
  while (Monitor(&err_msg)) {
    base::PlatformThread::Sleep(
        base::Milliseconds(sleep_time_milliseconds_));
  }

  if (terminate_) {
    VLOG(1) << "Monitor thread terminated gracefully.";
    return;
  }

  LOG(ERROR) << "Monitor thread terminated due to failure.";
  LOG(ERROR) << err_msg;
}

bool AbstractMonitor::Monitor(std::string *err_msg) {
  monitor_lock_.Acquire();

  while (!condition_) {
    wait_condition_.Wait();
    monitor_lock_.AssertAcquired();
  }

  if (terminate_) {
    monitor_lock_.Release();  // Probably unnecessary, but safe.
    return false;
  }

  monitor_lock_.Release();

  if (!VitalsExist()) {
    return Respond();
  }

  return true;
}

void AbstractMonitor::TerminateMonitorThread() {
  monitor_lock_.Acquire();

  // Indicate monitor thread should quit.
  terminate_ = true;

  // Ensure monitor thread exits condition variable.
  condition_ = true;
  monitor_lock_.Release();

  // Wake up monitor thread.
  wait_condition_.Broadcast();
}

void AbstractMonitor::DestroyMonitor() { TerminateMonitorThread(); }

}  // namespace huddly_monitor
