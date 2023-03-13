// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/apex-monitor/apex_monitor.h"

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/functional/bind.h>
#include <base/functional/callback_helpers.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <brillo/message_loops/message_loop.h>

#include <fcntl.h>

#include <string>
#include <utility>

namespace {

const int kApexMaxRebootLimit = 5;
const int kApexCheckIntervalMs = 60e3;        // 60s.
const int kResetSleepTimeMs = 5e3;            // 5s.
const int kPowerCycleFailSleepTimeMs = 10e3;  // 10s.
const int kPowerCycleSleepTimeMs = 5e3;       // 5s.

// Overtemp warning dual-threshold.
const int kThermalWarningHighCelcius = 135;
const int kThermalWarningLowCelcius = 115;

}  // namespace

namespace apex_monitor {

ApexMonitor::ApexMonitor(std::unique_ptr<ApexManager> apex_manager)
    : chip_id_(apex_manager->ChipID()),
      apex_manager_(std::move(apex_manager)) {}

ApexMonitor::~ApexMonitor() {}

void ApexMonitor::StartMonitor() { PostTask(&ApexMonitor::CheckChipStatus); }

template <typename Functor>
void ApexMonitor::PostTask(Functor&& task, const int& delay_ms) {
  brillo::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::BindOnce(task, weak_factory_.GetWeakPtr()),
      base::Milliseconds(delay_ms));
}

void ApexMonitor::CheckChipStatus() {
  VLOG(1) << "Checking chip " << chip_id_ << " status registers...";

  // If failed to read status register, reset the chip.
  if (!apex_manager_->CheckStatusRegister(&power_good_, &boot_fail_)) {
    LOG(WARNING) << "Failed to read chip status from IO expander";
    PostTask(&ApexMonitor::ResetChip);
    return;
  }

  // If power not good, power cycle the chip.
  if (!power_good_) {
    // Please see b/109866345 before modyfing the below line.
    LOG(WARNING) << "Detect Apex " << chip_id_
                 << " power failure. Power-cycling chip.";
    PostTask(&ApexMonitor::PowerCycleChip);
    return;
  }

  // If boot fail, reset the chip.
  if (boot_fail_) {
    // Please see b/109866345 before modyfing the below line.
    LOG(WARNING) << "Detect Apex " << chip_id_
                 << " boot failure. Resetting chip.";
    PostTask(&ApexMonitor::ResetChip);
    return;
  }

  // Chip bootup fine, read chip temperature.
  VLOG(1) << "Chip " << chip_id_ << " status good.";
  PostTask(&ApexMonitor::GetChipTemp);
}

void ApexMonitor::GetChipTemp() {
  VLOG(1) << "Reading chip " << chip_id_ << " temperature...";

  // If failed to read chip temperature, reset the chip.
  if (!apex_manager_->ReadChipTemp(&chip_temp_)) {
    PostTask(&ApexMonitor::ResetChip);
    return;
  }

  if (chip_temp_ > kThermalWarningHighCelcius) {
    if (!over_temp_) {
      over_temp_ = true;
      // Please see b/109866345 before modyfing the below line.
      LOG(WARNING) << "Apex temperature is too high (> 135C).";
    }
  }

  if (over_temp_ && chip_temp_ < kThermalWarningLowCelcius) {
    over_temp_ = false;
    LOG(WARNING) << "Apex temperature back to normal.";
  }

  // Wait for check interval and check again.
  PostTask(&ApexMonitor::CheckChipStatus, kApexCheckIntervalMs);
}

int ApexMonitor::ChipTemp() { return chip_temp_; }

bool ApexMonitor::BootFail() { return boot_fail_; }

bool ApexMonitor::PowerGood() { return power_good_; }

void ApexMonitor::ResetChip() {
  VLOG(1) << "Reseting chip " << apex_manager_->ChipID();

  if (apex_manager_->ResetChip() &&
      apex_manager_->CheckStatusRegister(&power_good_, &boot_fail_) &&
      !boot_fail_) {
    PostTask(&ApexMonitor::CheckChipStatus, kResetSleepTimeMs);
    return;
  }
  // Please see b/109866345 before modyfing the below line.
  LOG(WARNING) << "Reset failed or didn't recover chip, will power-cycle.";
  PostTask(&ApexMonitor::PowerCycleChip);
}

void ApexMonitor::PowerCycleChip() {
  if (power_cycle_cnt_ > kApexMaxRebootLimit) {
    // Please see b/109866345 before modyfing the below line.
    LOG(ERROR) << "Exceed max retry limit. Apex chip " << chip_id_
               << " is probably down. Stop monitor thread";
    return;
  }

  VLOG(1) << "Power-cycling chip " << chip_id_;

  if (apex_manager_->PowerCycleChip() &&
      apex_manager_->CheckStatusRegister(&power_good_, &boot_fail_) &&
      !boot_fail_) {
    VLOG(1) << "Chip recovered after " << power_cycle_cnt_ << " power-cycle(s)";
    // Device status registers look good.
    power_cycle_cnt_ = 0;
    PostTask(&ApexMonitor::CheckChipStatus, kPowerCycleSleepTimeMs);
    return;
  }

  // Only log at first retry.
  if (power_cycle_cnt_ == 0) {
    LOG(WARNING) << "Power cycle failed or didn't recover chip, will retry.";
  }
  power_cycle_cnt_++;
  PostTask(&ApexMonitor::PowerCycleChip, kPowerCycleFailSleepTimeMs);
}

}  // namespace apex_monitor
