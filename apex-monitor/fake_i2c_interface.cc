// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/apex-monitor/fake_i2c_interface.h"

#include <base/logging.h>

namespace {

const size_t kByteDataSize = 1;
const size_t kWordDataSize = 2;

// Thermal sensor CSRs.
const uint8_t kThermalSensorPageNumber = 0x00;
const uint8_t kThermalSensorClkEnAddr = 0xd0;
const uint8_t kThermalSensorPortEnAddr = 0xd8;
const uint8_t kThermalSensorCtrlEnAddr = 0xdc;
const uint8_t kThermalSensorTempByte0Addr = 0xde;
const uint8_t kThermalSensorTempByte1Addr = 0xdf;

// IO expander.
const uint8_t kApexResetSetAddr[] = {0x02, 0x03};
const uint8_t kApexResetSetBits[] = {0x03, 0x05};
// Apex power/boot state gpios.
const uint8_t kIoExpanderBootStateAddr[] = {0x00, 0x01};

const unsigned kRegisterDefaultValue = 0x00;

}  // namespace

namespace apex_monitor {

FakeI2cInterface::FakeI2cInterface(FakeRegisters* fake_registers)
    : registers(fake_registers) {
  registers->clear();
  registers->insert({kThermalSensorPageNumber,
                     {{kThermalSensorClkEnAddr, kRegisterDefaultValue},
                      {kThermalSensorPortEnAddr, kRegisterDefaultValue},
                      {kThermalSensorCtrlEnAddr, kRegisterDefaultValue},
                      {kThermalSensorTempByte0Addr, kRegisterDefaultValue},
                      {kThermalSensorTempByte1Addr, kRegisterDefaultValue}}});
  registers->insert({kPageNumberNone,
                     {{kIoExpanderBootStateAddr[0], kRegisterDefaultValue},
                      {kIoExpanderBootStateAddr[1], kRegisterDefaultValue},
                      {kApexResetSetAddr[0], kRegisterDefaultValue},
                      {kApexResetSetAddr[1], kRegisterDefaultValue}}});
}

FakeI2cInterface::~FakeI2cInterface() {}

std::unique_ptr<FakeI2cInterface> FakeI2cInterface::Create(
    FakeRegisters* fake_registers) {
  return std::make_unique<FakeI2cInterface>(fake_registers);
}

bool FakeI2cInterface::I2cWriteReg(const uint8_t& reg_addr,
                                   const unsigned char* buf,
                                   const uint16_t& write_size,
                                   const uint8_t& page_number) {
  if (write_size > 1) {
    LOG(WARNING) << "Do not support multi byte write.";
    return false;
  }

  ((*registers))[page_number][reg_addr] = *buf;

  return true;
}

bool FakeI2cInterface::I2cReadReg(const uint8_t& reg_addr, unsigned char* buf,
                                  const uint16_t& read_size,
                                  const uint8_t& page_number) {
  if (registers->empty()) {
    LOG(WARNING) << "Registers not initialized.";
    return false;
  }

  if (read_size > kWordDataSize ||
      ((read_size > kByteDataSize) &&
       (reg_addr != kThermalSensorTempByte0Addr))) {
    LOG(WARNING) << "Invalid read size.";
    return false;
  }

  *buf = (*registers)[page_number][reg_addr];
  if (read_size == kWordDataSize) {
    *(buf + kByteDataSize) =
        (*registers)[page_number][reg_addr + kByteDataSize];
  }

  return true;
}

bool FakeI2cInterface::SetBit(const uint8_t& reg_addr, const uint8_t& bit_id,
                              const uint8_t& page_number) {
  (*registers)[page_number][reg_addr] =
      ((*registers)[page_number][reg_addr] | (1 << bit_id));

  for (int i = 0; i < kApexChipCnt; ++i) {
    if (reg_addr == kApexResetSetAddr[i] && bit_id == kApexResetSetBits[i]) {
      reset_assert_count[i] += 1;
    }
  }

  return true;
}

bool FakeI2cInterface::ClearBit(const uint8_t& reg_addr, const uint8_t& bit_id,
                                const uint8_t& page_number) {
  (*registers)[page_number][reg_addr] =
      ((*registers)[page_number][reg_addr] & (~(1 << bit_id)));

  for (int i = 0; i < kApexChipCnt; ++i) {
    if (reg_addr == kApexResetSetAddr[i] && bit_id == kApexResetSetBits[i]) {
      reset_release_count[i] += 1;
    }
  }

  return true;
}

}  // namespace apex_monitor
