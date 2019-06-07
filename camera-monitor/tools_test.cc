// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/camera-monitor/tools.h"

#include <stdlib.h>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <brillo/test_helpers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

const char kFileEmptyPath[] = "camera-monitor/example/file_empty.txt";
const char kFileNoErrorPath[] = "camera-monitor/example/file_no_error.txt";
const char kFileWithErrorPath[] = "camera-monitor/example/file_with_error.txt";
const char kFileWithException[] =
    "camera-monitor/example/file_with_exception.txt";
const char kTestErrorKey[] = "TOOLS_UNITTEST_ERROR";
const char kTestErrorException[] = "TOOLS_UNITTEST_ERROR EXCEPT";
const char kTestNotPresentException[] = "TOOLS_UNITTEST_ERROR foo";

class ToolTest : public ::testing::Test {};

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

TEST_F(ToolTest, TestNullFile) {
  std::string err_msg;

  FILE *file_null = nullptr;

  EXPECT_FALSE(huddly_monitor::LookForErrorBlocking(
      kTestErrorKey, "", file_null, &err_msg));
  EXPECT_FALSE(err_msg.empty());
}

TEST_F(ToolTest, TestEmptyFile) {
  std::string err_msg;

  base::ScopedFILE file_empty(
      base::OpenFile(base::FilePath(kFileEmptyPath), "r"));

  EXPECT_FALSE(huddly_monitor::LookForErrorBlocking(
      kTestErrorKey, "", file_empty.get(), &err_msg));
  EXPECT_TRUE(err_msg.empty());
}

TEST_F(ToolTest, TestNonMatchingFile) {
  std::string err_msg;

  base::ScopedFILE file_no_error(
      base::OpenFile(base::FilePath(kFileNoErrorPath), "r"));

  EXPECT_FALSE(huddly_monitor::LookForErrorBlocking(
      kTestErrorKey, "", file_no_error.get(), &err_msg));
  EXPECT_TRUE(err_msg.empty());
}

TEST_F(ToolTest, TestMatchingFile) {
  std::string err_msg;

  base::ScopedFILE file_with_error(
      base::OpenFile(base::FilePath(kFileWithErrorPath), "r"));

  EXPECT_TRUE(huddly_monitor::LookForErrorBlocking(
      kTestErrorKey, "", file_with_error.get(), &err_msg));
  EXPECT_TRUE(err_msg.empty());
}

TEST_F(ToolTest, TestExceptionMatching) {
  std::string err_msg;

  base::ScopedFILE file_with_error(
      base::OpenFile(base::FilePath(kFileWithException), "r"));

  EXPECT_FALSE(huddly_monitor::LookForErrorBlocking(
      kTestErrorKey, kTestErrorException, file_with_error.get(), &err_msg));
  EXPECT_TRUE(err_msg.empty());
}

TEST_F(ToolTest, TestExceptionNotMatching) {
  std::string err_msg;

  base::ScopedFILE file_with_error(
      base::OpenFile(base::FilePath(kFileWithException), "r"));

  EXPECT_TRUE(huddly_monitor::LookForErrorBlocking(
      kTestErrorKey, kTestNotPresentException, file_with_error.get(),
      &err_msg));
  EXPECT_TRUE(err_msg.empty());
}

}  // namespace

int main(int argc, char **argv) {
  SetUpTests(&argc, argv, true);
  return RUN_ALL_TESTS();
}
