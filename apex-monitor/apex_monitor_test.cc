// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <brillo/message_loops/mock_message_loop.h>
#include <brillo/test_helpers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <utility>

#include "cfm-device-monitor/apex-monitor/apex_manager.h"
#include "cfm-device-monitor/apex-monitor/apex_monitor.h"
#include "cfm-device-monitor/apex-monitor/fake_i2c_interface.h"

namespace {

const uint8_t kByteDataSize = 1;
const uint8_t kPageNumberNone = 0xff;

// Apex I2C interface.
const uint8_t kThermalSensorPageNumber = 0x00;
const uint8_t kThermalSensorTempByte0Addr = 0xde;
const uint8_t kThermalSensorTempByte1Addr = 0xdf;
const uint8_t kThermalSensorTempByte0PreSetValueNormal = 0xf8;
const uint8_t kThermalSensorTempByte1PreSetValueNormal = 0x01;
const uint8_t kThermalSensorTempByte0PreSetValueHigh = 0x40;
const uint8_t kThermalSensorTempByte1PreSetValueHigh = 0x00;
const int kThermalSensorTempNormal = 40;
const int kThermalSensorTempHigh = 150;

// Apex power/boot state bits.
const uint8_t kIoExpanderPowerStateAddr = 0x01;
const uint8_t kIoExpanderPowerStateBit[] = {0x06, 0x07};
const uint8_t kIoExpanderBootStateAddr[] = {0x00, 0x01};
const uint8_t kIoExpanderBootStateBit[] = {0x00, 0x02};

using testing::_;

class ApexMonitorTest : public testing::Test {
 public:
  ApexMonitorTest() : loop_(nullptr) { loop_.SetAsCurrent(); }
  ApexMonitorTest(const ApexMonitorTest&) = delete;
  ApexMonitorTest& operator=(const ApexMonitorTest&) = delete;

  ~ApexMonitorTest() override = default;

 protected:
  brillo::MockMessageLoop loop_;
  FakeRegisters fake_registers;
};

TEST_F(ApexMonitorTest, TestCheckStatusFail) {
  // If ApexMonitor fails to read chip status bits, it should reset the chip.
  auto apex_interface = apex_monitor::FakeI2cInterface::Create(&fake_registers);
  auto io_exp_interface =
      apex_monitor::FakeI2cInterface::Create(&fake_registers);

  auto apex_manager = apex_monitor::ApexManager::Create(
      0, std::move(apex_interface), io_exp_interface.get());

  auto apex_monitor =
      std::make_unique<apex_monitor::ApexMonitor>(std::move(apex_manager));

  // Clear fake register table so check status will fail.
  fake_registers.clear();

  // Expecting post tasks 3 times in this test. one for CheckChipStatus, one
  // ResetChip and one for PowerCycleChip after ResetChip fails.
  EXPECT_CALL(loop_, PostDelayedTask(_, _, _)).Times(3);

  apex_monitor->StartMonitor();

  // Check status bits.
  loop_.RunOnce(true);

  EXPECT_EQ(io_exp_interface->reset_assert_count[0], 0);
  EXPECT_EQ(io_exp_interface->reset_release_count[0], 0);

  // Reset chip.
  loop_.RunOnce(true);

  EXPECT_EQ(io_exp_interface->reset_assert_count[0], 1);
  EXPECT_EQ(io_exp_interface->reset_release_count[0], 1);
}

TEST_F(ApexMonitorTest, TestPowerFailure) {
  // If power_good pin on IO expander is low, ApexMonitor should detect power
  // failure.
  auto apex_interface = apex_monitor::FakeI2cInterface::Create(&fake_registers);
  auto io_exp_interface =
      apex_monitor::FakeI2cInterface::Create(&fake_registers);

  auto apex_manager = apex_monitor::ApexManager::Create(
      0, std::move(apex_interface), io_exp_interface.get());

  auto apex_monitor =
      std::make_unique<apex_monitor::ApexMonitor>(std::move(apex_manager));

  // Set Apex0 power_good to low.
  io_exp_interface->ClearBit(kIoExpanderPowerStateAddr,
                             kIoExpanderPowerStateBit[0]);

  apex_monitor->StartMonitor();

  // Check status bits.
  loop_.RunOnce(true);

  EXPECT_FALSE(apex_monitor->PowerGood());
}

TEST_F(ApexMonitorTest, TestBootFailReset) {
  // If boot_fail pin on IO expander is high, apex monitor should be able to
  // detect that and reset chip accordingly.
  auto apex0_interface =
      apex_monitor::FakeI2cInterface::Create(&fake_registers);
  auto apex1_interface =
      apex_monitor::FakeI2cInterface::Create(&fake_registers);
  auto io_exp_interface =
      apex_monitor::FakeI2cInterface::Create(&fake_registers);

  auto apex0_manager = apex_monitor::ApexManager::Create(
      0, std::move(apex0_interface), io_exp_interface.get());
  auto apex1_manager = apex_monitor::ApexManager::Create(
      1, std::move(apex1_interface), io_exp_interface.get());

  auto apex0_monitor =
      std::make_unique<apex_monitor::ApexMonitor>(std::move(apex0_manager));
  auto apex1_monitor =
      std::make_unique<apex_monitor::ApexMonitor>(std::move(apex1_manager));

  // Set both chip status to boot_fail.
  for (int i = 0; i < 2; ++i) {
    io_exp_interface->SetBit(kIoExpanderBootStateAddr[i],
                             kIoExpanderBootStateBit[i]);
    io_exp_interface->SetBit(kIoExpanderPowerStateAddr,
                             kIoExpanderPowerStateBit[i]);
  }

  // Chip 0 boot_fail is true.
  EXPECT_TRUE(fake_registers[kPageNumberNone][kIoExpanderBootStateAddr[0]] &
              (1 << kIoExpanderBootStateBit[0]));
  // Chip 1 boot_fail is true.
  EXPECT_TRUE(fake_registers[kPageNumberNone][kIoExpanderBootStateAddr[1]] &
              (1 << kIoExpanderBootStateBit[1]));

  // Make sure there's no pending tasks.
  EXPECT_FALSE(loop_.fake_loop()->PendingTasks());

  apex0_monitor->StartMonitor();
  apex1_monitor->StartMonitor();

  EXPECT_EQ(io_exp_interface->reset_assert_count[0], 0);
  EXPECT_EQ(io_exp_interface->reset_release_count[0], 0);

  // Check status bits. Should read power_good = true, boot_fail = true.
  loop_.RunOnce(true);
  loop_.RunOnce(true);

  EXPECT_TRUE(apex0_monitor->PowerGood());
  EXPECT_TRUE(apex0_monitor->BootFail());
  EXPECT_TRUE(apex1_monitor->PowerGood());
  EXPECT_TRUE(apex1_monitor->BootFail());

  // Set chip0 boot_fail to false, mocking chip recovered.
  io_exp_interface->ClearBit(kIoExpanderBootStateAddr[0],
                             kIoExpanderBootStateBit[0]);

  // Apex 0 reset. Should see device recovered.
  loop_.RunOnce(true);

  EXPECT_EQ(io_exp_interface->reset_assert_count[0], 1);
  EXPECT_EQ(io_exp_interface->reset_release_count[0], 1);
  EXPECT_FALSE(apex0_monitor->BootFail());

  // Apex 1 reset.
  loop_.RunOnce(true);

  EXPECT_EQ(io_exp_interface->reset_assert_count[1], 1);
  EXPECT_EQ(io_exp_interface->reset_release_count[1], 1);
  EXPECT_TRUE(apex1_monitor->BootFail());
}

TEST_F(ApexMonitorTest, TestReadChipTemp) {
  // When the chip works fine, apex monitor should be able to read chip temp at
  // each check cycle.
  auto apex_interface = apex_monitor::FakeI2cInterface::Create(&fake_registers);
  auto io_exp_interface =
      apex_monitor::FakeI2cInterface::Create(&fake_registers);

  // Preset chip temperature to 40C.
  apex_interface->I2cWriteReg(kThermalSensorTempByte0Addr,
                              &kThermalSensorTempByte0PreSetValueNormal,
                              kByteDataSize, kThermalSensorPageNumber);
  apex_interface->I2cWriteReg(kThermalSensorTempByte1Addr,
                              &kThermalSensorTempByte1PreSetValueNormal,
                              kByteDataSize, kThermalSensorPageNumber);

  // Set chip status good.
  io_exp_interface->SetBit(kIoExpanderPowerStateAddr,
                           kIoExpanderPowerStateBit[0]);
  io_exp_interface->ClearBit(kIoExpanderBootStateAddr[0],
                             kIoExpanderBootStateBit[0]);

  auto apex_manager = apex_monitor::ApexManager::Create(
      0, std::move(apex_interface), io_exp_interface.get());

  auto apex_monitor =
      std::make_unique<apex_monitor::ApexMonitor>(std::move(apex_manager));

  // Make sure there's no pending tasks.
  EXPECT_FALSE(loop_.fake_loop()->PendingTasks());

  apex_monitor->StartMonitor();

  // Check chip status.
  loop_.RunOnce(true);

  EXPECT_EQ(apex_monitor->ChipTemp(), 0);

  // Read chip temperature.
  loop_.RunOnce(true);

  EXPECT_EQ(apex_monitor->ChipTemp(), kThermalSensorTempNormal);

  // Change chip temperature to 150C.
  fake_registers[kThermalSensorPageNumber][kThermalSensorTempByte0Addr] =
      kThermalSensorTempByte0PreSetValueHigh;
  fake_registers[kThermalSensorPageNumber][kThermalSensorTempByte1Addr] =
      kThermalSensorTempByte1PreSetValueHigh;

  // Check chip status.
  loop_.RunOnce(true);
  // Read chip temperature.
  loop_.RunOnce(true);

  EXPECT_EQ(apex_monitor->ChipTemp(), kThermalSensorTempHigh);
}

}  // namespace

int main(int argc, char** argv) {
  SetUpTests(&argc, argv, true);
  return RUN_ALL_TESTS();
}
