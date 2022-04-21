// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <brillo/syslog_logging.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "cfm-device-monitor/apex-monitor/apex_manager.h"
#include "cfm-device-monitor/apex-monitor/apex_monitor.h"

namespace {

const char i2c_root_bus_dir[] =
    "/sys/bus/pci/devices/0000:00:15.2/i2c_designware.1/";

const uint8_t kApex0I2cSlaveAddr = 0x10;
const uint8_t kApex1I2cSlaveAddr = 0x11;
const uint8_t kIoExpanderSlaveAddr = 0x74;

const int kApexI2cBusCnt = 4;
constexpr int kApex0I2cBusIdx = 0;
constexpr int kApex1I2cBusIdx = 2;

}  // namespace

// Utility function used to get the I2C bus number corresponding to
// IO expander, Apex0 and Apex1 I2C interfaces.
bool GetI2cBusNumber(int* root_bus_number, int* apex0_bus_number,
                     int* apex1_bus_number) {
  // Get root I2C bus number, which is the bus number for the IO expander.
  base::FilePath root_i2c_dir(i2c_root_bus_dir);
  base::FileEnumerator root_i2c_enum(
      root_i2c_dir, false, base::FileEnumerator::DIRECTORIES, "i2c-?");

  base::FilePath root_i2c_path = root_i2c_enum.Next().StripTrailingSeparators();
  if (root_i2c_path.empty()) {
    LOG(ERROR) << "Failed to get i2c root bus number.";
    return false;
  }
  *root_bus_number = root_i2c_path.value().back() - '0';

  // Get I2C bus numbers for Apex I2C interfaces.
  std::vector<base::FilePath::StringType> path_components;
  std::vector<int> apex_i2c_bus_numbers;

  base::FileEnumerator apex_i2c_enum(
      root_i2c_path, false, base::FileEnumerator::DIRECTORIES, "i2c-*");
  for (base::FilePath apex_path =
           apex_i2c_enum.Next().StripTrailingSeparators();
       !apex_path.empty();
       apex_path = apex_i2c_enum.Next().StripTrailingSeparators()) {
    path_components = apex_path.GetComponents();
    if (path_components.back().substr(4) == "dev") {
      continue;
    }
    apex_i2c_bus_numbers.push_back(std::stoi(path_components.back().substr(4)));
  }

  if (apex_i2c_bus_numbers.size() != kApexI2cBusCnt) {
    LOG(ERROR) << "Unexpected Apex I2C bus count.";
    return false;
  }

  std::sort(apex_i2c_bus_numbers.begin(), apex_i2c_bus_numbers.end());
  *apex0_bus_number = apex_i2c_bus_numbers[kApex0I2cBusIdx];
  *apex1_bus_number = apex_i2c_bus_numbers[kApex1I2cBusIdx];

  return true;
}

int main(int argc, char* argv[]) {
  // Configure logging to syslog.
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  LOG(INFO) << "Apex-monitor start...";

  int root_i2c_number, apex0_i2c_number, apex1_i2c_number;
  GetI2cBusNumber(&root_i2c_number, &apex0_i2c_number, &apex1_i2c_number);

  auto apex0_i2c_interface =
      apex_monitor::I2cInterface::Create(apex0_i2c_number, kApex0I2cSlaveAddr);
  auto apex1_i2c_interface =
      apex_monitor::I2cInterface::Create(apex1_i2c_number, kApex1I2cSlaveAddr);
  auto io_expander_interface =
      apex_monitor::I2cInterface::Create(root_i2c_number, kIoExpanderSlaveAddr);

  if (!apex0_i2c_interface || !apex1_i2c_interface || !io_expander_interface) {
    LOG(ERROR) << "Failed to init I2C interfaces.";
    return 1;
  }

  apex_monitor::ApexManager::InitIoExpander(io_expander_interface.get());

  auto apex0_manager = apex_monitor::ApexManager::Create(
      0, std::move(apex0_i2c_interface), io_expander_interface.get());
  auto apex1_manager = apex_monitor::ApexManager::Create(
      1, std::move(apex1_i2c_interface), io_expander_interface.get());

  if (!apex0_manager || !apex1_manager) {
    LOG(ERROR) << "Failed to init apex manager.";
    return 1;
  }

  auto apex0_monitor =
      std::make_unique<apex_monitor::ApexMonitor>(std::move(apex0_manager));
  auto apex1_monitor =
      std::make_unique<apex_monitor::ApexMonitor>(std::move(apex1_manager));

  if (!apex0_monitor || !apex1_monitor) {
    LOG(ERROR) << "Failed to init apex monitors.";
    return 1;
  }

  return apex_monitor::ApexMonitorDaemon(std::move(apex0_monitor),
                                         std::move(apex1_monitor))
      .Run();
}
