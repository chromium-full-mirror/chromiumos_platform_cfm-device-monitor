// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APEX_MONITOR_FAKE_I2C_INTERFACE_H_
#define APEX_MONITOR_FAKE_I2C_INTERFACE_H_

#include <memory>
#include <unordered_map>

#include "cfm-device-monitor/apex-monitor/i2c_interface.h"

constexpr int kApexChipCnt = 2;

// Use a hashmap to store all the register values. Mock I2C read/write.
using FakeRegisters =
    std::unordered_map<uint8_t /* Page number */,
                       std::unordered_map<uint8_t /* Register addr */,
                                          unsigned char /* Register value */>>;

namespace apex_monitor {

class FakeI2cInterface : public BaseI2cInterface {
 public:
  explicit FakeI2cInterface(FakeRegisters* fake_registers);
  FakeI2cInterface(const FakeI2cInterface&) = delete;
  FakeI2cInterface& operator=(const FakeI2cInterface&) = delete;

  ~FakeI2cInterface();

  static std::unique_ptr<FakeI2cInterface> Create(
      FakeRegisters* fake_registers);

  int reset_assert_count[kApexChipCnt] = {0};
  int reset_release_count[kApexChipCnt] = {0};

  FakeRegisters* registers;

  bool I2cWriteReg(const uint8_t& reg_addr, const unsigned char* buf,
                   const uint16_t& write_size,
                   const uint8_t& page_number = kPageNumberNone) override;
  bool I2cReadReg(const uint8_t& reg_addr, unsigned char* buf,
                  const uint16_t& read_size,
                  const uint8_t& page_number = kPageNumberNone) override;
  bool SetBit(const uint8_t& reg_addr, const uint8_t& bit_id,
              const uint8_t& page_number = kPageNumberNone) override;
  bool ClearBit(const uint8_t& reg_addr, const uint8_t& bit_id,
                const uint8_t& page_number = kPageNumberNone) override;
};

}  // namespace apex_monitor

#endif  // APEX_MONITOR_FAKE_I2C_INTERFACE_H_
