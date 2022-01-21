// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APEX_MONITOR_APEX_MANAGER_H_
#define APEX_MONITOR_APEX_MANAGER_H_

#include <memory>

#include "cfm-device-monitor/apex-monitor/i2c_interface.h"

namespace apex_monitor {

class ApexManager {
 public:
  explicit ApexManager(const int& chip_id,
                       std::unique_ptr<BaseI2cInterface> apex_interface,
                       BaseI2cInterface* ioexp_interface);
  ApexManager(const ApexManager&) = delete;
  ApexManager& operator=(const ApexManager&) = delete;

  ~ApexManager();

  static std::unique_ptr<ApexManager> Create(
      const int& chip_id, std::unique_ptr<BaseI2cInterface> apex_interface,
      BaseI2cInterface* ioexp_interface);

  // Read Apex chip status bits on IO expander.
  bool CheckStatusRegister(bool* power_good, bool* boot_fail);
  // Read chip temperature from internal thermal sensor.
  // Temperature in Celsius degree.
  bool ReadChipTemp(int* chip_temp);
  // Toggle chip reset bit to reset Apex chip.
  bool ResetChip();
  // Toggle Apex power control GPIOs to power cycle chip.
  bool PowerCycleChip();
  // Check if IO expander IO directions configured as expected.
  bool IoExpanderConfigGood();

  int ChipID();

  // Init IO expander, set the IO directions.
  static bool InitIoExpander(BaseI2cInterface* ioexp_interface);
  // Init current IO expander interface.
  bool InitIoExpander();

 private:
  int chip_id_;

  std::unique_ptr<BaseI2cInterface> apex_i2c_interface_ = nullptr;
  // Hold a raw pointer of the IO expander interface in order to
  // share among all ApexManager instances.
  BaseI2cInterface* io_expander_interface_;

  bool InitThermalSensor();
  bool ExportPowerCtrlGpio();
  inline void TempCodeToCelsius(const uint16_t& temp_code, int* temp);
};

}  // namespace apex_monitor

#endif  // APEX_MONITOR_APEX_MANAGER_H_
