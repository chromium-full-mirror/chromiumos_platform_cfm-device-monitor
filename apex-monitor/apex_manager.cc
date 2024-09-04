// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/apex-monitor/apex_manager.h"

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>

#include <fcntl.h>

#include <string>
#include <utility>

#include "cfm-device-monitor/apex-monitor/i2c_interface.h"

namespace {

const size_t kByteDataSize = 1;
const size_t kWordDataSize = 2;

// Thermal sensor CSRs.
const uint8_t kThermalSensorPageNumber = 0x00;
// Thermal sensor clock enable: bit 7 of 0xd0.
const uint8_t kThermalSensorClkEnAddr = 0xd0;
const uint8_t kThermalSensorClkEnBit = 0x7;
// Thermal sensor ports enable: last 3 bit of 0xd8.
const uint8_t kThermalSensorPortEnAddr = 0xd8;
const uint8_t kThermalSensorPortEnadBit = 0x2;
const uint8_t kThermalSensorPortEnvrBit = 0x1;
const uint8_t kThermalSensorPortEnbgBit = 0x0;
// Thermal sensor OMC controller enable: bit 0 of 0xdc.
const uint8_t kThermalSensorCtrlEnAddr = 0xdc;
const unsigned char kThermalSensorCtrlEnBit = 0x0;
// Temperature data: 10 bits starting at 0xde.
const uint8_t kThermalSensorTempAddr = 0xde;
const uint16_t kThermalSensorTempMask = 0x03ff;
// If temperature < -50C. It's more likely the thermal sensor is
// not configured properly.
const int kThermalLowestThreshold = -50;

const uint32_t kEnableThermalControllerSlpTimeUs = 1e5;

// IO expander.
const uint8_t kApexResetSetAddr[] = {0x02, 0x03};
// Reset bits are bit 03 and 15.
const uint8_t kApexResetSetBits[] = {0x03, 0x05};
// Wait 100ms before release reset signal.
const uint32_t kApexResetSlpTimeUs = 100e3;
// IO expander IO configuration registers.
const uint8_t kIoExpanderIoConfig0Addr = 0x06;
const uint8_t kIoExpanderIoConfig1Addr = 0x07;
const unsigned char kIoExpanderIoConfig0Value = 0x67;
const unsigned char kIoExpanderIoConfig1Value = 0xdc;
// Apex power/boot state bits.
const uint8_t kIoExpanderPowerStateAddr = 0x01;
const unsigned char kApexPowerStateBitMask[] = {0x40, 0x80};
const uint8_t kIoExpanderBootStateAddr[] = {0x00, 0x01};
const unsigned char kApexBootStateBitMask[] = {0x01, 0x04};

// Power control GPIOs.
const char kSysfsGpioPath[] = "/sys/class/gpio/";
constexpr int kApexPowerCtrlGpioId[] = {416, 417};
const char kGpioDirOutStr[] = "out";
const char kGpioValueHigh[] = "1";
const char kGpioValueLow[] = "0";
// Wait 1s for power cycle.
const uint32_t kApexPowerCycleSlpTimeUs = 1e6;

}  // namespace

namespace apex_monitor {

ApexManager::ApexManager(const int& chip_id,
                         std::unique_ptr<BaseI2cInterface> apex_interface,
                         BaseI2cInterface* ioexp_interface)
    : chip_id_(chip_id),
      apex_i2c_interface_(std::move(apex_interface)),
      io_expander_interface_(ioexp_interface) {}

ApexManager::~ApexManager() {}

std::unique_ptr<ApexManager> ApexManager::Create(
    const int& chip_id, std::unique_ptr<BaseI2cInterface> apex_interface,
    BaseI2cInterface* ioexp_interface) {
  auto apex_manager = std::make_unique<ApexManager>(
      chip_id, std::move(apex_interface), ioexp_interface);
  if (!apex_manager->InitThermalSensor()) {
    LOG(ERROR) << "Failed to init thermal sensor for chip " << chip_id;
    return nullptr;
  }

  return apex_manager;
}

bool ApexManager::InitThermalSensor() {
  // Enable thermal sensor clock.
  if (!apex_i2c_interface_->SetBit(kThermalSensorClkEnAddr,
                                   kThermalSensorClkEnBit,
                                   kThermalSensorPageNumber)) {
    LOG(ERROR) << "Failed to enable thermal sensor clock.";
    return false;
  }

  // Enable thermal sensor input port: ENBG, ENVR, ENAD.
  if (!(apex_i2c_interface_->SetBit(kThermalSensorPortEnAddr,
                                    kThermalSensorPortEnadBit,
                                    kThermalSensorPageNumber) &&
        apex_i2c_interface_->SetBit(kThermalSensorPortEnAddr,
                                    kThermalSensorPortEnvrBit,
                                    kThermalSensorPageNumber) &&
        apex_i2c_interface_->SetBit(kThermalSensorPortEnAddr,
                                    kThermalSensorPortEnbgBit,
                                    kThermalSensorPageNumber))) {
    LOG(ERROR) << "Failed to enable thermal sensor input ports.";
    return false;
  }

  usleep(kEnableThermalControllerSlpTimeUs);

  // Enable OMC thermal sensor controller.
  if (!apex_i2c_interface_->SetBit(kThermalSensorCtrlEnAddr,
                                   kThermalSensorCtrlEnBit,
                                   kThermalSensorPageNumber)) {
    LOG(ERROR) << "Failed to enable thermal sensor controller.";
    return false;
  }

  return true;
}

bool ApexManager::InitIoExpander(BaseI2cInterface* ioexp_interface) {
  if (!ioexp_interface->I2cWriteReg(kIoExpanderIoConfig0Addr,
                                    &kIoExpanderIoConfig0Value,
                                    kByteDataSize)) {
    LOG(ERROR) << "Failed to configure IO expander byte 0.";
    return false;
  }

  if (!ioexp_interface->I2cWriteReg(kIoExpanderIoConfig1Addr,
                                    &kIoExpanderIoConfig1Value,
                                    kByteDataSize)) {
    LOG(ERROR) << "Failed to configure IO expander byte 1.";
    return false;
  }

  return true;
}

bool ApexManager::InitIoExpander() {
  if (!InitIoExpander(io_expander_interface_)) {
    LOG(ERROR) << "Failed to init IO expander.";
    return false;
  }

  return true;
}

bool ApexManager::IoExpanderConfigGood() {
  unsigned char config_data;
  if (!io_expander_interface_->I2cReadReg(kIoExpanderIoConfig0Addr,
                                          &config_data, kByteDataSize)) {
    LOG(ERROR) << "Failed to read IO expander configuration byte 0.";
    return false;
  }
  if (config_data != kIoExpanderIoConfig0Value) return false;

  if (!io_expander_interface_->I2cReadReg(kIoExpanderIoConfig1Addr,
                                          &config_data, kByteDataSize)) {
    LOG(ERROR) << "Failed to read IO expander configuration byte 1.";
    return false;
  }
  if (config_data != kIoExpanderIoConfig1Value) return false;

  return true;
}

bool ApexManager::CheckStatusRegister(bool* power_good, bool* boot_fail) {
  unsigned char read_data;
  // Read power status.
  if (!io_expander_interface_->I2cReadReg(kIoExpanderPowerStateAddr, &read_data,
                                          kByteDataSize)) {
    LOG(ERROR) << "Failed to read power status.";
    return false;
  }
  *power_good = read_data & kApexPowerStateBitMask[chip_id_];

  // If power_good is false, check boot status doesn't make any sense.
  if (!power_good) {
    *boot_fail = true;
    return true;
  }

  // Read boot status. The register address of boot status and power status
  // are the same for apex chip 1. So no need to re-read the register data
  // in that case.
  if (kIoExpanderBootStateAddr[chip_id_] != kIoExpanderPowerStateAddr) {
    if (!io_expander_interface_->I2cReadReg(kIoExpanderBootStateAddr[chip_id_],
                                            &read_data, kByteDataSize)) {
      LOG(ERROR) << "Failed to read boot status.";
      return false;
    }
  }
  *boot_fail = read_data & kApexBootStateBitMask[chip_id_];

  return true;
}

int ApexManager::ChipID() { return chip_id_; }

bool ApexManager::ReadChipTemp(int* temp) {
  uint16_t temp_data;
  if (!apex_i2c_interface_->I2cReadReg(
          kThermalSensorTempAddr, reinterpret_cast<unsigned char*>(&temp_data),
          kWordDataSize, kThermalSensorPageNumber)) {
    LOG(ERROR) << "Failed to read temperature of Apex " << chip_id_;
    return false;
  }
  TempCodeToCelsius(temp_data, temp);

  // If the temprature is too low, it more likely that the thermal
  // sensor configuration is wrong.
  if (*temp < kThermalLowestThreshold) {
    InitThermalSensor();
    return true;
  }

  LOG(INFO) << "Apex chip " << chip_id_ << " temperature: " << *temp;

  return true;
}

inline void ApexManager::TempCodeToCelsius(const uint16_t& temp_code,
                                           int* temp) {
  *temp = static_cast<int>(
      (((662.0 - (temp_code & kThermalSensorTempMask)) / 4.0 + 0.5) * 10.0 +
       0.5) /
      10);
}

bool ApexManager::ResetChip() {
  // First check IO expander IO configuration.
  if (!IoExpanderConfigGood()) {
    if (!InitIoExpander()) {
      LOG(ERROR) << "Invalid IO expander IO direction configuration.";
      return false;
    }
  }

  // Assert reset signal.
  if (!io_expander_interface_->ClearBit(kApexResetSetAddr[chip_id_],
                                        kApexResetSetBits[chip_id_])) {
    LOG(ERROR) << "Failed to assert reset signal for apex " << chip_id_;
    return false;
  }

  usleep(kApexResetSlpTimeUs);

  // Release reset signal.
  if (!io_expander_interface_->SetBit(kApexResetSetAddr[chip_id_],
                                      kApexResetSetBits[chip_id_])) {
    LOG(ERROR) << "Failed to release reset signal for apex " << chip_id_;
    return false;
  }

  // Re-init thermal sensor registers.
  if (!InitThermalSensor()) {
    LOG(ERROR) << "Failed to re-init thermal sensor.";
    return false;
  }

  return true;
}

bool ApexManager::ExportPowerCtrlGpio() {
  std::string chip_id_str = std::to_string(kApexPowerCtrlGpioId[chip_id_]);
  base::FilePath gpio_path(kSysfsGpioPath + std::string("gpio") + chip_id_str);
  if (base::PathExists(gpio_path)) {
    VLOG(1) << "GPIO " << chip_id_str << " already exported.";
    return true;
  }

  // Export target GPIO.
  base::FilePath export_path(kSysfsGpioPath + std::string("export"));
  base::ScopedFD export_fd(
      open(export_path.value().c_str(), O_WRONLY | O_CLOEXEC));
  if (!export_fd.is_valid()) {
    LOG(ERROR) << "Failed to open " << export_path.value();
    return false;
  }

  int r = write(export_fd.get(), chip_id_str.c_str(), chip_id_str.size());
  if (r < 0 || !base::PathExists(gpio_path)) {
    PLOG(ERROR) << "Failed to export GPIO " << chip_id_str;
    return false;
  }

  if (!base::WriteFile(gpio_path.Append("direction"), kGpioDirOutStr)) {
    PLOG(ERROR) << "Failed to set GPIO direction for GPIO"
                << kApexPowerCtrlGpioId[chip_id_];
    return false;
  }

  return true;
}

bool ApexManager::PowerCycleChip() {
  if (!ExportPowerCtrlGpio()) {
    LOG(ERROR) << "Failed to export GPIOs to control apex power.";
    return false;
  }

  base::FilePath gpio_path(kSysfsGpioPath + std::string("gpio") +
                           std::to_string(kApexPowerCtrlGpioId[chip_id_]));

  // Power off chip.
  if (!base::WriteFile(gpio_path.Append("value"), kGpioValueLow)) {
    PLOG(ERROR) << "Failed to set GPIO " << kApexPowerCtrlGpioId[chip_id_]
                << " value to low.";
    return false;
  }

  // Check IO expander IO configuration before toggling reset bit.
  if (!IoExpanderConfigGood()) {
    if (!InitIoExpander()) {
      LOG(ERROR) << "Invalid IO expander IO direction configuration.";
      return false;
    }
  }

  // Assert reset signal.
  if (!io_expander_interface_->ClearBit(kApexResetSetAddr[chip_id_],
                                        kApexResetSetBits[chip_id_])) {
    LOG(ERROR) << "Failed to set reset signal for apex " << chip_id_;
    return false;
  }

  usleep(kApexPowerCycleSlpTimeUs);

  // Power on chip.
  if (!base::WriteFile(gpio_path.Append("value"), kGpioValueHigh)) {
    PLOG(ERROR) << "Failed to set GPIO " << kApexPowerCtrlGpioId[chip_id_]
                << " value to high.";
    return false;
  }

  usleep(kApexResetSlpTimeUs);

  // Release reset signal.
  if (!io_expander_interface_->SetBit(kApexResetSetAddr[chip_id_],
                                      kApexResetSetBits[chip_id_])) {
    LOG(ERROR) << "Failed to release reset signal for apex " << chip_id_;
    return false;
  }

  // Re-init thermal sensor.
  if (!InitThermalSensor()) {
    LOG(ERROR) << "Failed to re-init thermal sensor.";
    return false;
  }

  return true;
}

}  // namespace apex_monitor
