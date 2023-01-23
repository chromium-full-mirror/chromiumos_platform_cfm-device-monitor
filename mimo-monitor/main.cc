// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/functional/bind.h>
#include <base/logging.h>
#include <base/process/process_iterator.h>
#include <brillo/message_loops/message_loop.h>
#include <brillo/syslog_logging.h>

#include <errno.h>
#include <libusb-1.0/libusb.h>
#include <sys/file.h>
#include <string>

#include "cfm-device-monitor/mimo-monitor/mimo_monitor.h"

int main(int argc, char *argv[]) {
  // Configure logging to syslog.
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  VLOG(1) << "Mimo-monitor start...";

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
