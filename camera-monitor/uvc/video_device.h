// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAMERA_MONITOR_UVC_VIDEO_DEVICE_H_
#define CAMERA_MONITOR_UVC_VIDEO_DEVICE_H_

#include <optional>

#include <base/files/file.h>
#include <base/posix/eintr_wrapper.h>
#include <linux/uvcvideo.h>
#include <sys/ioctl.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "cfm-device-monitor/camera-monitor/uvc/utilities.h"

namespace cfm {
namespace uvc {

const char kDefaultVideoDeviceMountPoint[] = "/sys/class/video4linux";
const char kDefaultVideoDevicePoint[] = "device/..";

class VideoDevice {
 public:
  // The Delegate interface performs work on behalf of VideoDevice.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual bool FindDevicesWrapper(uint16_t vendor_id, uint16_t product_id,
                                    std::vector<base::FilePath> *devices) = 0;
    virtual int Ioctl(int fd, int request, uvc_xu_control_query* query) = 0;
  };
  explicit VideoDevice(uint16_t vendor_id, uint16_t product_id);

  explicit VideoDevice(uint16_t vendor_id, uint16_t product_id,
                       std::unique_ptr<Delegate> delegate);

  VideoDevice(const VideoDevice&) = delete;
  VideoDevice& operator=(const VideoDevice&) = delete;

  virtual ~VideoDevice();

  std::optional<uint8_t> GetUnitId() { return unit_id_; }

  void SetUnitId(uint8_t unit_id) {
    unit_id_ = std::optional<uint8_t>(unit_id);
  }

  virtual bool IsValid();

  virtual bool OpenDevice();
  virtual void CloseDevice();

  virtual bool QueryXuControl(unsigned char control_selector, uint8_t *data,
                              uint8_t query_request, uint16_t size);

  virtual bool GetXuControlLength(unsigned char control_selector,
                                  uint16_t* length);

  virtual bool GetXuControl(unsigned char control_selector,
                            std::vector<unsigned char> *data);

  virtual bool SetXuControl(unsigned char control_selector,
                            std::vector<unsigned char> data);

 private:
  uint16_t vendor_id_;
  uint16_t product_id_;
  std::optional<uint8_t> unit_id_;
  std::unique_ptr<Delegate> delegate_;
  base::File file_;
  int file_descriptor_;
};

}  // namespace uvc
}  // namespace cfm

#endif  // CAMERA_MONITOR_UVC_VIDEO_DEVICE_H_
