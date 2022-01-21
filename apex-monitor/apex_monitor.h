// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APEX_MONITOR_APEX_MONITOR_H_
#define APEX_MONITOR_APEX_MONITOR_H_

#include <base/threading/thread.h>
#include <brillo/daemons/daemon.h>

#include <memory>
#include <utility>
#include <vector>

#include "cfm-device-monitor/apex-monitor/apex_manager.h"
#include "cfm-device-monitor/apex-monitor/i2c_interface.h"

namespace apex_monitor {

class ApexMonitor {
 public:
  explicit ApexMonitor(std::unique_ptr<ApexManager> apex_manager);
  ApexMonitor(const ApexMonitor&) = delete;
  ApexMonitor& operator=(const ApexMonitor&) = delete;

  ~ApexMonitor();

  void StartMonitor();

  // Return chip temperature, for testing purpose.
  int ChipTemp();
  // Return chip boot status, for testing purpose.
  bool BootFail();
  // Return chip power status, for testing purpose.
  bool PowerGood();

 private:
  int chip_id_ = 0;
  int chip_temp_ = 0;
  // Count for how many times the chip has retried to power-cycle.
  int power_cycle_cnt_ = 0;
  bool boot_fail_ = false;
  bool power_good_ = true;
  // An over temperature bit to flag when the chip thermal status change.
  bool over_temp_ = false;

  std::unique_ptr<ApexManager> apex_manager_;

  base::WeakPtrFactory<ApexMonitor> weak_factory_{this};

  // A wrapper for brillo::Post(Delayed)Task.
  template <typename Functor>
  void PostTask(Functor&& task, const int& delay_ms = 0);

  // Check Apex chip status bits: power_good and boot_fail.
  void CheckChipStatus();
  // Read current chip temperature from the internal thermal sensor.
  // Temperature in Celsius degree.
  void GetChipTemp();
  // Toggle chip reset bit to reset Apex chip.
  void ResetChip();
  // Power cycle the chip. Stop monitoring if the chip can't be
  // recovered after max power cycle retry limit.
  void PowerCycleChip();
};

class ApexMonitorDaemon : public brillo::Daemon {
 public:
  explicit ApexMonitorDaemon(std::unique_ptr<ApexMonitor> apex0_monitor,
                             std::unique_ptr<ApexMonitor> apex1_monitor)
      : apex0_monitor_(std::move(apex0_monitor)),
        apex1_monitor_(std::move(apex1_monitor)) {}
  ApexMonitorDaemon(const ApexMonitorDaemon&) = delete;
  ApexMonitorDaemon& operator=(const ApexMonitorDaemon&) = delete;

 protected:
  int OnInit() override {
    apex0_monitor_->StartMonitor();
    apex1_monitor_->StartMonitor();
    return 0;
  }

 private:
  std::unique_ptr<ApexMonitor> apex0_monitor_{nullptr};
  std::unique_ptr<ApexMonitor> apex1_monitor_{nullptr};
};

}  // namespace apex_monitor

#endif  // APEX_MONITOR_APEX_MONITOR_H_
