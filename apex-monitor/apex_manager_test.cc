// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <brillo/test_helpers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <utility>

#include "cfm-device-monitor/apex-monitor/apex_manager.h"
#include "cfm-device-monitor/apex-monitor/fake_i2c_interface.h"

namespace {

const size_t kByteDataSize = 1;

// Apex I2C interface.
const uint8_t kThermalSensorPageNumber = 0x00;
const uint8_t kThermalSensorTempByte0Addr = 0xde;
const uint8_t kThermalSensorTempByte1Addr = 0xdf;
const uint8_t kThermalSensorTempByte0PreSetValueNormal = 0xf8;
const uint8_t kThermalSensorTempByte1PreSetValueNormal = 0x01;
const uint8_t kThermalSensorTempByte0PreSetValueHigh = 0x40;
const uint8_t kThermalSensorTempByte1PreSetValueHigh = 0x00;
const int kThermalSensorPreSetTempNormal = 40;
const int kThermalSensorPreSetTempHigh = 150;

// IO expander.
// Apex power/boot state gpios.
const uint8_t kIoExpanderPowerStateAddr = 0x01;
const uint8_t kIoExpanderPowerStateBit[] = {0x06, 0x07};
const uint8_t kIoExpanderBootStateAddr[] = {0x00, 0x01};
const uint8_t kIoExpanderBootStateBit[] = {0x00, 0x02};
const uint8_t kIoExpanderIoConfig0Addr = 0x06;
const uint8_t kIoExpanderIoConfig1Addr = 0x07;
const unsigned char kIoExpanderIoConfig0Value = 0x67;
const unsigned char kIoExpanderIoConfig1Value = 0xdc;

const uint8_t kPageNumberNone = 0xff;

class ApexManagerTest : public ::testing::Test {
 protected:
  FakeRegisters fake_registers;
};

TEST_F(ApexManagerTest, TestCheckStatusRegisterFail) {
  auto apex_interface = apex_monitor::FakeI2cInterface::Create(&fake_registers);
  auto io_exp_interface =
      apex_monitor::FakeI2cInterface::Create(&fake_registers);

  auto apex0_manager = apex_monitor::ApexManager::Create(
      0, std::move(apex_interface), io_exp_interface.get());

  // Clear register table so read will fail.
  fake_registers.clear();

  bool power_good = true;
  bool boot_fail = true;

  // I2C read will fail. CheckStatusRegister should return false.
  EXPECT_FALSE(apex0_manager->CheckStatusRegister(&power_good, &boot_fail));
}

TEST_F(ApexManagerTest, TestCheckStatusRegister) {
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

  bool power_good = true;
  bool boot_fail = true;

  // power_good, boot_fail bits are by default false.
  EXPECT_TRUE(apex0_manager->CheckStatusRegister(&power_good, &boot_fail));
  EXPECT_FALSE(power_good);
  EXPECT_FALSE(boot_fail);
  apex1_manager->CheckStatusRegister(&power_good, &boot_fail);
  EXPECT_FALSE(power_good);
  EXPECT_FALSE(boot_fail);

  // Set power_good, boot_fail bits to true.
  for (int i = 0; i < 2; ++i) {
    io_exp_interface->SetBit(kIoExpanderBootStateAddr[i],
                             kIoExpanderBootStateBit[i]);
    io_exp_interface->SetBit(kIoExpanderPowerStateAddr,
                             kIoExpanderPowerStateBit[i]);
  }

  // ApexManager should read power_good and boot_fail true.
  apex0_manager->CheckStatusRegister(&power_good, &boot_fail);
  EXPECT_TRUE(power_good);
  EXPECT_TRUE(boot_fail);
  apex1_manager->CheckStatusRegister(&power_good, &boot_fail);
  EXPECT_TRUE(power_good);
  EXPECT_TRUE(boot_fail);
}

TEST_F(ApexManagerTest, TestReadChipTempNormal) {
  auto apex_interface = apex_monitor::FakeI2cInterface::Create(&fake_registers);
  auto io_exp_interface =
      apex_monitor::FakeI2cInterface::Create(&fake_registers);

  // Set fake apex_interface temperature to 40C.
  apex_interface->I2cWriteReg(kThermalSensorTempByte0Addr,
                              &kThermalSensorTempByte0PreSetValueNormal,
                              kByteDataSize, kThermalSensorPageNumber);
  apex_interface->I2cWriteReg(kThermalSensorTempByte1Addr,
                              &kThermalSensorTempByte1PreSetValueNormal,
                              kByteDataSize, kThermalSensorPageNumber);

  auto apex_manager = apex_monitor::ApexManager::Create(
      0, std::move(apex_interface), io_exp_interface.get());

  // Read chip temp, should get 40C.
  int chip_temp;
  apex_manager->ReadChipTemp(&chip_temp);
  EXPECT_EQ(chip_temp, kThermalSensorPreSetTempNormal);
}

TEST_F(ApexManagerTest, TestReadChipTempHigh) {
  auto apex_interface = apex_monitor::FakeI2cInterface::Create(&fake_registers);
  auto io_exp_interface =
      apex_monitor::FakeI2cInterface::Create(&fake_registers);

  // Set fake apex_interface temperature to 150C, which is higher than
  // threshold 135C.
  apex_interface->I2cWriteReg(kThermalSensorTempByte0Addr,
                              &kThermalSensorTempByte0PreSetValueHigh,
                              kByteDataSize, kThermalSensorPageNumber);
  apex_interface->I2cWriteReg(kThermalSensorTempByte1Addr,
                              &kThermalSensorTempByte1PreSetValueHigh,
                              kByteDataSize, kThermalSensorPageNumber);

  auto apex_manager = apex_monitor::ApexManager::Create(
      0, std::move(apex_interface), io_exp_interface.get());

  int chip_temp;
  apex_manager->ReadChipTemp(&chip_temp);
  EXPECT_EQ(chip_temp, kThermalSensorPreSetTempHigh);
}

TEST_F(ApexManagerTest, TestResetChip) {
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

  // ResetChip asserts reset bit and then release it.
  apex0_manager->ResetChip();
  EXPECT_EQ(io_exp_interface->reset_assert_count[0], 1);
  EXPECT_EQ(io_exp_interface->reset_assert_count[0],
            io_exp_interface->reset_release_count[0]);
  apex1_manager->ResetChip();
  EXPECT_EQ(io_exp_interface->reset_assert_count[1], 1);
  EXPECT_EQ(io_exp_interface->reset_assert_count[1],
            io_exp_interface->reset_release_count[1]);
}

TEST_F(ApexManagerTest, TestInitIoExpander) {
  auto io_exp_interface =
      apex_monitor::FakeI2cInterface::Create(&fake_registers);

  // InitIoExpander set the IO expander IO configuration bytes.
  apex_monitor::ApexManager::InitIoExpander(io_exp_interface.get());
  EXPECT_EQ(fake_registers[kPageNumberNone][kIoExpanderIoConfig0Addr],
            kIoExpanderIoConfig0Value);
  EXPECT_EQ(fake_registers[kPageNumberNone][kIoExpanderIoConfig1Addr],
            kIoExpanderIoConfig1Value);
}

TEST_F(ApexManagerTest, TestIoExpanderConfigGood) {
  auto apex_interface = apex_monitor::FakeI2cInterface::Create(&fake_registers);
  auto io_exp_interface =
      apex_monitor::FakeI2cInterface::Create(&fake_registers);

  // Set both IO expander configuration bytes invalid.
  unsigned char wront_config_byte = 0xaa;
  io_exp_interface->I2cWriteReg(kIoExpanderIoConfig0Addr,
                                &kIoExpanderIoConfig0Value, kByteDataSize);
  io_exp_interface->I2cWriteReg(kIoExpanderIoConfig1Addr, &wront_config_byte,
                                kByteDataSize);

  auto apex_manager = apex_monitor::ApexManager::Create(
      0, std::move(apex_interface), io_exp_interface.get());

  EXPECT_FALSE(apex_manager->IoExpanderConfigGood());

  // Set one of IO expander configuration bytes invalid.
  io_exp_interface->I2cWriteReg(kIoExpanderIoConfig0Addr, &wront_config_byte,
                                kByteDataSize);
  io_exp_interface->I2cWriteReg(kIoExpanderIoConfig1Addr,
                                &kIoExpanderIoConfig1Value, kByteDataSize);
  EXPECT_FALSE(apex_manager->IoExpanderConfigGood());

  // Set both IO expander configuration bytes valid.
  io_exp_interface->I2cWriteReg(kIoExpanderIoConfig0Addr,
                                &kIoExpanderIoConfig0Value, kByteDataSize);
  io_exp_interface->I2cWriteReg(kIoExpanderIoConfig1Addr,
                                &kIoExpanderIoConfig1Value, kByteDataSize);
  EXPECT_TRUE(apex_manager->IoExpanderConfigGood());
}

}  // namespace

int main(int argc, char** argv) {
  SetUpTests(&argc, argv, true);
  return RUN_ALL_TESTS();
}
