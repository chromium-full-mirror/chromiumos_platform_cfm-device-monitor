// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/camera-monitor/tools.h"
#include <brillo/test_helpers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdlib.h>

namespace {

// Made up vid, pid that could not possibly belong to any device.
const uint16_t kBogusVid = 9999;
const uint16_t kBogusPid = 9999;

// Vid, pid belonging to Linux root hub, present in all Linux and chrome
// machines.
const uint16_t kRealVid = 0x1d6b;
const uint16_t kRealPid = 0x0002;

const char kFileEmptyPath[] = "camera-monitor/example/file_empty.txt";
const char kFileNoErrorPath[] = "camera-monitor/example/"
                                "file_no_error.txt";
const char kFileWithErrorPath[] = "camera-monitor/example/file_with_error.txt";
const char kTestErrorKey[] = "TOOLS_UNITTEST_ERROR";

class ToolTest : public ::testing::Test {
 protected:
  // void SetUp() override { }
  // void TearDown() override { }
};

TEST_F(ToolTest, GetDeviceNoDeviceReturnFalseAndNullptr) {
  libusb_device *device = nullptr;
  std::string err_msg = "";
  // Bogus vid and bogus pid.
  EXPECT_FALSE(
      huddly_monitor::GetDevice(kBogusVid, kBogusPid, &device, &err_msg));
  EXPECT_EQ(device, nullptr);
  EXPECT_TRUE(err_msg.empty());

  // Only bogus vid.
  EXPECT_FALSE(
      huddly_monitor::GetDevice(kBogusVid, kRealPid, &device, &err_msg));
  EXPECT_EQ(device, nullptr);
  EXPECT_TRUE(err_msg.empty());

  // Only bogus pid.
  EXPECT_FALSE(
      huddly_monitor::GetDevice(kRealVid, kBogusPid, &device, &err_msg));
  EXPECT_EQ(device, nullptr);
  EXPECT_TRUE(err_msg.empty());
}

TEST_F(ToolTest, GetDeviceReturnTrueAndRightDevice) {
  libusb_device *device = nullptr;
  std::string err_msg;
  libusb_device_descriptor descriptor;

  EXPECT_TRUE(huddly_monitor::GetDevice(kRealVid, kRealPid, &device, &err_msg));
  EXPECT_NE(device, nullptr);

  // Get descriptor and assert it succeeded before using it.
  int ret = libusb_get_device_descriptor(device, &descriptor);
  ASSERT_EQ(ret, 0);

  EXPECT_EQ(descriptor.idVendor, kRealVid);
  EXPECT_EQ(descriptor.idProduct, kRealPid);
  EXPECT_TRUE(err_msg.empty());
}

TEST_F(ToolTest, TestGetGpioNum) {
  // Values for bus 1.
  EXPECT_EQ(huddly_monitor::GetGpioNum(1, 2), 218);
  EXPECT_EQ(huddly_monitor::GetGpioNum(1, 3), 219);
  EXPECT_EQ(huddly_monitor::GetGpioNum(1, 5), 209);
  EXPECT_EQ(huddly_monitor::GetGpioNum(1, 6), 209);
  // Invalid value.
  EXPECT_EQ(huddly_monitor::GetGpioNum(1, 1), 0);

  // Values for bus 2.
  EXPECT_EQ(huddly_monitor::GetGpioNum(2, 1), 218);
  EXPECT_EQ(huddly_monitor::GetGpioNum(2, 2), 219);
  EXPECT_EQ(huddly_monitor::GetGpioNum(2, 3), 209);
  EXPECT_EQ(huddly_monitor::GetGpioNum(2, 4), 209);

  // Invalid values.
  EXPECT_EQ(huddly_monitor::GetGpioNum(2, 5), 0);
  EXPECT_EQ(huddly_monitor::GetGpioNum(3, 2), 0);
}

TEST_F(ToolTest, TestLookForErrorBlockingAll) {
  // Setup.
  std::string err_msg;
  FILE *file_null = nullptr;

  FILE *file_empty = fopen(kFileEmptyPath, "r");
  ASSERT_NE(file_empty, nullptr)
      << "Failed to open test file " << kFileEmptyPath << ". Check path.";

  FILE *file_no_error = fopen(kFileNoErrorPath, "r");
  ASSERT_NE(file_no_error, nullptr)
      << "Failed to open test file " << kFileNoErrorPath << ". Check path.";
  ASSERT_FALSE(ferror(file_no_error));

  FILE *file_with_error = fopen(kFileWithErrorPath, "r");
  ASSERT_NE(file_empty, nullptr)
      << "Failed to open test file " << kFileWithErrorPath << ". Check path.";
  ASSERT_FALSE(ferror(file_with_error));

  EXPECT_FALSE(
      huddly_monitor::LookForErrorBlocking(kTestErrorKey, file_null, &err_msg));
  EXPECT_FALSE(err_msg.empty());
  err_msg = "";

  EXPECT_FALSE(huddly_monitor::LookForErrorBlocking(kTestErrorKey, file_empty,
                                                    &err_msg));
  EXPECT_TRUE(err_msg.empty());
  err_msg = "";

  EXPECT_FALSE(huddly_monitor::LookForErrorBlocking(kTestErrorKey,
                                                    file_no_error, &err_msg));
  EXPECT_TRUE(err_msg.empty());
  err_msg = "";

  EXPECT_TRUE(huddly_monitor::LookForErrorBlocking(kTestErrorKey,
                                                   file_with_error, &err_msg));
  EXPECT_TRUE(err_msg.empty());

  // Teardown.
  // No need to fclose file_null_, since it is null.
  fclose(file_empty);
  fclose(file_no_error);
  fclose(file_with_error);
}

}  // namespace

int main(int argc, char **argv) {
  SetUpTests(&argc, argv, true);
  return RUN_ALL_TESTS();
}
