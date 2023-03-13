// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/functional/bind.h>
#include <base/logging.h>
#include <base/time/time.h>
#include <brillo/daemons/daemon.h>
#include <brillo/syslog_logging.h>
#include <getopt.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include "cfm-device-monitor/camera-monitor/huddly_monitor.h"
#include "cfm-device-monitor/camera-monitor/udev.h"

constexpr auto kMetricsStartDelay = base::Milliseconds(500);
constexpr auto kMetricsTimeInterval = base::Minutes(15);

class HuddlyDaemon : public brillo::Daemon {
 public:
  HuddlyDaemon() : monitor_object_(false, 500) {
    monitor_object_.StartMonitor();
    monitor_object_.StartMetricsLog(kMetricsStartDelay, kMetricsTimeInterval);
  }
  HuddlyDaemon(const HuddlyDaemon&) = delete;
  HuddlyDaemon& operator=(const HuddlyDaemon&) = delete;

 protected:
  int OnEventLoopStarted() override {
    udev_ = std::make_unique<huddly_monitor::Udev>(
        base::BindRepeating(&HuddlyDaemon::OnCameraAdded,
                            weak_factory_.GetWeakPtr()),
        base::BindRepeating(&HuddlyDaemon::OnCameraRemoved,
                            weak_factory_.GetWeakPtr()));

    if (udev_->IsCameraConnected()) {
      OnCameraAdded();
    }

    return brillo::Daemon::OnEventLoopStarted();
  }

 private:
  void OnCameraAdded() {
    monitor_object_.UpdateCondition(true);
  }

  void OnCameraRemoved() {
    monitor_object_.UpdateCondition(false);
  }

  huddly_monitor::HuddlyMonitor monitor_object_;

  // Udev object used to communicate with libudev.
  std::unique_ptr<huddly_monitor::Udev> udev_;

  base::WeakPtrFactory<HuddlyDaemon> weak_factory_{this};
};

int main(int argc, char *argv[]) {
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);
  logging::LoggingSettings logging_settings;
  logging::InitLogging(logging_settings);

  VLOG(1) << "Starting huddly monitor.";
  return HuddlyDaemon().Run();
}
