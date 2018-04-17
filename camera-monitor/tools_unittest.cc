// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/camera-monitor/tools.h"
#include <brillo/test_helpers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdlib.h>

namespace {

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

TEST_F(ToolTest, TestGetGpioNumGuado) {
  // Values for bus 1.
  EXPECT_EQ(huddly_monitor::GetGpioNumGuado(1, 2), 218);
  EXPECT_EQ(huddly_monitor::GetGpioNumGuado(1, 3), 219);
  EXPECT_EQ(huddly_monitor::GetGpioNumGuado(1, 5), 209);
  EXPECT_EQ(huddly_monitor::GetGpioNumGuado(1, 6), 209);
  // Invalid value.
  EXPECT_EQ(huddly_monitor::GetGpioNumGuado(1, 1), 0);

  // Values for bus 2.
  EXPECT_EQ(huddly_monitor::GetGpioNumGuado(2, 1), 218);
  EXPECT_EQ(huddly_monitor::GetGpioNumGuado(2, 2), 219);
  EXPECT_EQ(huddly_monitor::GetGpioNumGuado(2, 3), 209);
  EXPECT_EQ(huddly_monitor::GetGpioNumGuado(2, 4), 209);

  // Invalid values.
  EXPECT_EQ(huddly_monitor::GetGpioNumGuado(2, 5), 0);
  EXPECT_EQ(huddly_monitor::GetGpioNumGuado(3, 2), 0);
}

TEST_F(ToolTest, TestGetUsbPortIdFizz) {
  EXPECT_EQ(huddly_monitor::GetUsbPortIdFizz(1, 5), 0);
  EXPECT_EQ(huddly_monitor::GetUsbPortIdFizz(1, 6), 1);
  EXPECT_EQ(huddly_monitor::GetUsbPortIdFizz(1, 2), 2);
  EXPECT_EQ(huddly_monitor::GetUsbPortIdFizz(1, 3), 3);
  EXPECT_EQ(huddly_monitor::GetUsbPortIdFizz(1, 4), 4);

  // Invalid values.
  EXPECT_EQ(huddly_monitor::GetUsbPortIdFizz(1, 1), UINT8_MAX);
  EXPECT_EQ(huddly_monitor::GetUsbPortIdFizz(1, 7), UINT8_MAX);
  EXPECT_EQ(huddly_monitor::GetUsbPortIdFizz(0, 5), UINT8_MAX);
  EXPECT_EQ(huddly_monitor::GetUsbPortIdFizz(2, 5), UINT8_MAX);
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
