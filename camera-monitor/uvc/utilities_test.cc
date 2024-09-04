// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/camera-monitor/uvc/utilities.h"

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace cfm {
namespace uvc {

const char kDevicePoint[] = "device_point/";
const uint16_t kVendorId = 0x1111;
const char kVendorIdString[] = "1111";
const uint16_t kProductId = 0x2222;
const char kProductIdString[] = "2222";

class UtilitiesTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    dir_path_ = temp_dir_.GetPath();
    ASSERT_TRUE(temp_dir_.Delete());
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    dev_dir_ = temp_dir_.GetPath();
    WriteTestFile(dir_path_.Append("A"), kVendorIdString, kProductIdString);
    WriteTestFile(dir_path_.Append("B"), kProductIdString, kVendorIdString);
    WriteTestFile(dir_path_.Append("C"), "gibberish", "blah");
  }

  // Writes test file in a directory.
  void WriteTestFile(const base::FilePath &file_path, const char* vid,
                     const char* pid) {
    base::FilePath device_point_path = file_path.Append(kDevicePoint);
    ASSERT_TRUE(base::CreateDirectory(device_point_path));
    base::FilePath vendor_id_path = device_point_path.Append("idVendor");
    ASSERT_TRUE(base::WriteFile(vendor_id_path, vid));
    base::FilePath product_id_path = device_point_path.Append("idProduct");
    ASSERT_TRUE(base::WriteFile(product_id_path, pid));
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath dir_path_;
  base::FilePath dev_dir_;
};

TEST_F(UtilitiesTest, GetContentsForInvalidDirectory) {
  ASSERT_TRUE(base::DeletePathRecursively(dir_path_));
  ASSERT_FALSE(base::DirectoryExists(dir_path_));
  std::vector<std::string> contents;
  EXPECT_FALSE(GetDirectoryContents(dir_path_.value(), &contents));
}

TEST_F(UtilitiesTest, GetContentsForValidDirectory) {
  std::vector<std::string> contents;
  EXPECT_TRUE(GetDirectoryContents(dir_path_.value(), &contents));
  EXPECT_THAT(contents,
              ::testing::UnorderedElementsAre(".", "..", "A", "B", "C"));
}

TEST_F(UtilitiesTest, FindDevicesForInvalidDirectory) {
  ASSERT_TRUE(base::DeletePathRecursively(dir_path_));
  ASSERT_FALSE(base::DirectoryExists(dir_path_));
  std::vector<base::FilePath> devices;
  EXPECT_FALSE(FindDevices(dir_path_.value(), kDevicePoint, dev_dir_, kVendorId,
                           kProductId, &devices));
}

TEST_F(UtilitiesTest, FindDevicesHasNoMountPoint) {
  std::vector<base::FilePath> devices;
  EXPECT_FALSE(
      FindDevices("", kDevicePoint, dev_dir_, kVendorId, kProductId, &devices));
}

TEST_F(UtilitiesTest, FindDevicesHasNoDevicePoint) {
  std::vector<base::FilePath> devices;
  EXPECT_FALSE(FindDevices(dir_path_.value(), "", dev_dir_, kVendorId,
                           kProductId, &devices));
}

TEST_F(UtilitiesTest, FindDevicesForValidDirectory) {
  std::vector<base::FilePath> devices;
  EXPECT_TRUE(FindDevices(dir_path_.value(), kDevicePoint, dev_dir_, kVendorId,
                          kProductId, &devices));
  EXPECT_EQ(devices.size(), 1);
  EXPECT_EQ(devices.at(0).value(), dev_dir_.Append("A").value());
}

}  // namespace uvc
}  // namespace cfm
