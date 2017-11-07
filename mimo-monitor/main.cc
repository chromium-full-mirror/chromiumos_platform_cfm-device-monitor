// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/bind.h>
#include <base/logging.h>
#include <base/process/process_iterator.h>
#include <brillo/message_loops/message_loop.h>
#include <brillo/syslog_logging.h>

#include <errno.h>
#include <libusb-1.0/libusb.h>
#include <sys/file.h>
#include <string>

#include "cfm-device-monitor/mimo-monitor/mimo_monitor.h"

namespace {
const char kPidFilePath[] = "/tmp/mimo-monitor.pid";
}

int main(int argc, char *argv[]) {
  // Configure logging to syslog.
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  VLOG(1) << "Mimo-monitor start...";

  // If the file opening fails, the lock will also fail. So omit check here.
  int pid_file = open(kPidFilePath, O_CREAT | O_RDWR, 0666);
  int lock_result = flock(pid_file, LOCK_EX | LOCK_NB);
  if (lock_result && errno == EWOULDBLOCK) {
    VLOG(1) << "Another mimo-monitor is running. Exiting...";
    return 0;
  }

  std::unique_ptr<mimo_monitor::MimoMonitor> monitord =
      mimo_monitor::MimoMonitor::Create();
  if (!monitord) {
    LOG(ERROR) << "Failed to create mimo monitor object.";
    exit(EXIT_FAILURE);
  }

  monitord->TaskScheduler();

  monitord->Run();

  return 0;
}
