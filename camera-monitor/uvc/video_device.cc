// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/camera-monitor/uvc/video_device.h"

#include <base/files/file.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <fcntl.h>
#include <linux/usb/video.h>
#include <linux/uvcvideo.h>
#include <linux/videodev2.h>
#include <string.h>

#include <algorithm>
#include <utility>

namespace cfm {
namespace uvc {

namespace {

class RealDelegate : public VideoDevice::Delegate {
 public:
  RealDelegate() = default;
  RealDelegate(const RealDelegate&) = delete;
  RealDelegate& operator=(const RealDelegate&) = delete;

  bool FindDevicesWrapper(uint16_t vendor_id,
                          uint16_t product_id,
                          std::vector<base::FilePath>
                          *devices) {
    return FindDevices(kDefaultVideoDeviceMountPoint,
                       kDefaultVideoDevicePoint,
                       base::FilePath("/dev/"),
                       vendor_id, product_id, devices);
  }

  int Ioctl(int fd, int request, uvc_xu_control_query* query) {
    return HANDLE_EINTR(ioctl(fd, request, query));
  }
};

}  // namespace

VideoDevice::VideoDevice(uint16_t vendor_id, uint16_t product_id)
  : VideoDevice(vendor_id, product_id, std::make_unique<RealDelegate>()) {}

VideoDevice::VideoDevice(uint16_t vendor_id, uint16_t product_id,
                         std::unique_ptr<Delegate> delegate)
    : vendor_id_(vendor_id), product_id_(product_id),
      delegate_(std::move(delegate)), file_descriptor_(-1) {}

VideoDevice::~VideoDevice() {
  if (IsValid()) {
    CloseDevice();
  }
}

bool VideoDevice::IsValid() {
  return file_.IsValid();
}

bool VideoDevice::OpenDevice() {
  if (IsValid()) {
    return true;
  }
  std::vector<base::FilePath> dev_paths;
  if (!delegate_->FindDevicesWrapper(vendor_id_, product_id_, &dev_paths)) {
    LOG(ERROR) << "Failed to find devices";
    return false;
  }
  if (dev_paths.empty()) {
    LOG(ERROR) << "No device path found.";
    return false;
  }
  base::FilePath dev_path = dev_paths.at(0);
  int fd = open(dev_path.value().c_str(), O_RDWR | O_NONBLOCK, 0);
  if (fd == -1) {
    LOG(ERROR) << "Failed to open device. Open file failed."
               << dev_path.value().c_str();
    return false;
  }
  LOG(INFO) << "File path: " << dev_path.value();
  file_ = base::File(dev_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  file_descriptor_ = fd;
  return true;
}

void VideoDevice::CloseDevice() {
  if (file_descriptor_ >= 0) {
    close(file_descriptor_);
  }
  file_.Close();
  file_descriptor_ = -1;
}

bool VideoDevice::QueryXuControl(unsigned char control_selector,
                                 uint8_t* data,
                                 uint8_t query_request,
                                 uint16_t size) {
  if (!file_.IsValid()) {
    LOG(ERROR) << "File is invalid when querying xu control.";
    return false;
  }

  if (!unit_id_.has_value()) {
    LOG(ERROR) << "Unit id is not set.";
    return false;
  }

  struct uvc_xu_control_query control_query;
  control_query.unit = *unit_id_;
  control_query.selector = control_selector;
  control_query.query = query_request;
  control_query.size = size;
  control_query.data = data;
  int error =
      delegate_->Ioctl(file_descriptor_, UVCIOC_CTRL_QUERY, &control_query);

  if (error < 0) {
    LOG(ERROR) << "ioctl call failed. " << strerror(errno);
    CloseDevice();
    return false;
  }
  return true;
}

bool VideoDevice::GetXuControlLength(unsigned char control_selector,
                                     uint16_t* length) {
  return QueryXuControl(control_selector, reinterpret_cast<uint8_t*>(length),
                     UVC_GET_LEN, sizeof(uint16_t));
}

bool VideoDevice::GetXuControl(unsigned char control_selector,
                               std::vector<unsigned char>* data) {
  uint16_t data_len;
  if (!GetXuControlLength(control_selector, &data_len)) {
    LOG(ERROR) << "Failed to get xu control length.";
    return false;
  }
  data->resize(data_len);
  int error =
      QueryXuControl(control_selector, data->data(), UVC_GET_CUR, data_len);
  if (error < 0) {
    LOG(ERROR) << "Failed to get xu control data.";
    return false;
  }
  return true;
}

bool VideoDevice::SetXuControl(unsigned char control_selector,
                               std::vector<unsigned char> data) {
  int error =
      QueryXuControl(control_selector, data.data(), UVC_SET_CUR, data.size());
  if (error < 0) {
    LOG(ERROR) << "Failed to set xu control data.";
    return false;
  }
  return true;
}

}  // namespace uvc
}  // namespace cfm
