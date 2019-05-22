// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAMERA_MONITOR_UVC_VIDEO_DEVICE_H_
#define CAMERA_MONITOR_UVC_VIDEO_DEVICE_H_

#include <base/files/file.h>
#include <base/macros.h>
#include <cstdint>
#include <string>
#include <vector>

namespace cfm {
namespace uvc {

class VideoDevice {
 public:
  VideoDevice(uint16_t vendor_id, uint16_t product_id);
  virtual ~VideoDevice();

  uint8_t GetUnitId() { return unit_id_; }

  void SetUnitId(uint8_t unit_id) { unit_id_ = unit_id; }

  virtual bool IsValid();

  virtual bool OpenDevice();
  virtual void CloseDevice();

  virtual bool QueryXuControl(unsigned char control_selector,
                              uint8_t* data,
                              uint8_t query_request,
                              uint16_t size);

  virtual uint16_t GetXuControlLength(unsigned char control_selector);

  virtual bool GetXuControl(unsigned char control_selector,
                            std::vector<unsigned char>* data);

  virtual bool SetXuControl(unsigned char control_selector,
                            std::vector<unsigned char> data);

 private:
  uint16_t vendor_id_;
  uint16_t product_id_;
  uint8_t unit_id_;
  base::File file_;
  int file_descriptor_;

  DISALLOW_COPY_AND_ASSIGN(VideoDevice);
};

}  // namespace uvc
}  // namespace cfm

#endif  // CAMERA_MONITOR_UVC_VIDEO_DEVICE_H_
