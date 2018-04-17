// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "brillo/syslog_logging.h"
#include "cfm-device-monitor/camera-monitor/huddly_monitor.h"

int main(int argc, char *argv[]) {
  brillo::InitLog(brillo::kLogToStderr);
  // Required for base::SysInfo.
  base::AtExitManager at_exit_manager;

  huddly_monitor::HuddlyMonitor monitor(false, 500);
  // Try to hot plug the camera.
  return (monitor.Respond() ? 0 : 1);
}
