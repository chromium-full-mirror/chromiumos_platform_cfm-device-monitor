// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/camera-monitor/uvc/video_device.h"

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <brillo/test_helpers.h>
#include <gtest/gtest.h>
#include <linux/usb/video.h>
#include <linux/uvcvideo.h>

namespace cfm {
namespace uvc {

const char kDevicePoint[] = "device";
const uint16_t kVendorId = 0x1111;
const char kVendorIdString[] = "1111";
const uint16_t kProductId = 0x2222;
const char kProductIdString[] = "2222";
const uint8_t kUnitId = 3;
const unsigned char kControlSelector = 0x04;

class StubDelegate : public VideoDevice::Delegate {
 public:
  StubDelegate();
  StubDelegate(std::string mount_point, base::FilePath dev_dir)
      : mount_point_(mount_point), dev_dir_(dev_dir) {}

  StubDelegate(const StubDelegate&) = delete;
  StubDelegate& operator=(const StubDelegate&) = delete;

  void reset() {
    num_ioctls_ = 0;
    fd_ = 0;
    request_ = 0;
    query_ = nullptr;
  }
  void set_data(std::vector<unsigned char> data) {
    data_ = data;
  }
  int num_ioctls() const { return num_ioctls_; }
  int get_fd() const { return fd_; }
  unsigned int get_request() const { return request_; }
  struct uvc_xu_control_query* get_query() { return query_; }

  bool FindDevicesWrapper(uint16_t vendor_id, uint16_t product_id,
                          std::vector<base::FilePath> *dev_paths) {
    return FindDevices(mount_point_, kDevicePoint, dev_dir_, vendor_id,
                       product_id, dev_paths);
  }
  int Ioctl(int fd, int request, uvc_xu_control_query* query) {
    num_ioctls_++;
    fd_ = fd;
    request_ = request;
    query_ = query;
    for (int i = 0; i < data_.size(); i++) {
      EXPECT_EQ(query_->data[i], data_[i]);
    }
    return true;
  }

 private:
  std::string mount_point_;
  base::FilePath dev_dir_;
  int num_ioctls_;
  int fd_;
  unsigned int request_;
  struct uvc_xu_control_query* query_;
  std::vector<unsigned char> data_;
};

class VideoDeviceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    mount_path_ = temp_dir_.GetPath();
    ASSERT_TRUE(temp_dir_.Delete());
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    dev_dir_ = temp_dir_.GetPath();
    delegate_ = new StubDelegate(mount_path_.value(), dev_dir_);
    WriteTestFile("A", kVendorIdString, kProductIdString);
    WriteTestFile("B", kProductIdString, kVendorIdString);
    WriteTestFile("C", "gibberish", "blah");
    video_device_ =
      new VideoDevice(kVendorId, kProductId, base::WrapUnique(delegate_));
  }

  // Writes test file in a directory.
  void WriteTestFile(const char* file, const char* vid,
                     const char* pid) {
    base::FilePath device_point_path =
        mount_path_.Append(file).Append(kDevicePoint);
    ASSERT_TRUE(base::CreateDirectory(device_point_path));
    base::FilePath vendor_id_path = device_point_path.Append("idVendor");
    ASSERT_TRUE(base::WriteFile(vendor_id_path, vid));
    base::FilePath product_id_path = device_point_path.Append("idProduct");
    ASSERT_TRUE(base::WriteFile(product_id_path, pid));
    base::FilePath dev_file_path = dev_dir_.Append(file);
    ASSERT_TRUE(base::WriteFile(dev_file_path, ""));
    LOG(ERROR) << dev_file_path.value();
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath mount_path_;
  base::FilePath dev_dir_;
  StubDelegate *delegate_;
  VideoDevice *video_device_;
};

TEST_F(VideoDeviceTest, SetUnitId) {
  video_device_->SetUnitId(kUnitId);
  EXPECT_EQ(*(video_device_->GetUnitId()), kUnitId);
}

TEST_F(VideoDeviceTest, OpenDeviceNoPath) {
  ASSERT_TRUE(base::DeletePathRecursively(mount_path_));
  ASSERT_FALSE(base::DirectoryExists(mount_path_));
  EXPECT_FALSE(video_device_->OpenDevice());
}

TEST_F(VideoDeviceTest, OpenDeviceNoDevice) {
  VideoDevice *no_matching_pid_ =
    new VideoDevice(kVendorId, kVendorId, base::WrapUnique(delegate_));
  EXPECT_FALSE(no_matching_pid_->OpenDevice());
}

TEST_F(VideoDeviceTest, IsValidAndOpenDevice) {
  EXPECT_FALSE(video_device_->IsValid());
  EXPECT_TRUE(video_device_->OpenDevice());
  EXPECT_TRUE(video_device_->IsValid());
}

TEST_F(VideoDeviceTest, GetControlLengthNoDevice) {
  VideoDevice *no_matching_pid_ =
    new VideoDevice(kVendorId, kVendorId, base::WrapUnique(delegate_));
  uint16_t length;
  EXPECT_FALSE(no_matching_pid_->OpenDevice());
  EXPECT_FALSE(no_matching_pid_->GetXuControlLength(kControlSelector, &length));
}

TEST_F(VideoDeviceTest, GetControlLengthNoUnitId) {
  uint16_t length;
  EXPECT_TRUE(video_device_->OpenDevice());
  EXPECT_FALSE(video_device_->GetXuControlLength(kControlSelector, &length));
}

TEST_F(VideoDeviceTest, GetControlLength) {
  delegate_->reset();
  uint16_t length;
  EXPECT_TRUE(video_device_->OpenDevice());
  video_device_->SetUnitId(kUnitId);
  EXPECT_TRUE(video_device_->GetXuControlLength(kControlSelector, &length));
  EXPECT_EQ(delegate_->num_ioctls(), 1);
  EXPECT_EQ(delegate_->get_request(), UVCIOC_CTRL_QUERY);
  uvc_xu_control_query* query = delegate_->get_query();
  EXPECT_EQ(query->size, sizeof(uint16_t));
  EXPECT_EQ(query->unit, kUnitId);
  EXPECT_EQ(query->query, UVC_GET_LEN);
  EXPECT_EQ(query->data, reinterpret_cast<uint8_t*>(&length));
  EXPECT_EQ(query->selector, kControlSelector);
}

TEST_F(VideoDeviceTest, GetControl) {
  delegate_->reset();
  std::vector<unsigned char> data;
  EXPECT_TRUE(video_device_->OpenDevice());
  video_device_->SetUnitId(kUnitId);
  EXPECT_TRUE(video_device_->GetXuControl(kControlSelector, &data));
  EXPECT_EQ(delegate_->num_ioctls(), 2);
  EXPECT_EQ(delegate_->get_request(), UVCIOC_CTRL_QUERY);
  uvc_xu_control_query* query = delegate_->get_query();
  EXPECT_EQ(query->unit, kUnitId);
  EXPECT_EQ(query->query, UVC_GET_CUR);
  EXPECT_EQ(query->data, data.data());
  EXPECT_EQ(query->selector, kControlSelector);
}

TEST_F(VideoDeviceTest, SetControl) {
  delegate_->reset();
  std::vector<unsigned char> data {'a', 'b', 'c', 'd'};
  delegate_->set_data(data);
  EXPECT_TRUE(video_device_->OpenDevice());
  video_device_->SetUnitId(kUnitId);
  EXPECT_TRUE(video_device_->SetXuControl(kControlSelector, data));
  EXPECT_EQ(delegate_->num_ioctls(), 1);
  EXPECT_EQ(delegate_->get_request(), UVCIOC_CTRL_QUERY);
  struct uvc_xu_control_query* query = delegate_->get_query();
  EXPECT_EQ(query->unit, kUnitId);
  EXPECT_EQ(query->query, UVC_SET_CUR);
  EXPECT_EQ(query->selector, kControlSelector);
  EXPECT_EQ(query->size, data.size());
}

}  //  namespace uvc
}  //  namespace cfm
