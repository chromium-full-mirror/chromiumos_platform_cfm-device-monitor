// Copyright 2017 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/mimo-monitor/displaylink_monitor.h"

#include <base/logging.h>
#include <base/threading/platform_thread.h>
#include <base/time/time.h>
#include <brillo/syslog_logging.h>
#include <libusb-1.0/libusb.h>

#include <stdio.h>
#include <unistd.h>
#include <string>

#include "cfm-device-monitor/mimo-monitor/utils.h"

namespace {

constexpr uint16_t kIdVendor = 0x17e9;
// Product ID for Mimo Vue.
constexpr uint16_t kIdProductVue = 0x016b;
// Product ID for Mimo Capture.
constexpr uint16_t kIdProductCapture = 0x416d;
constexpr uint16_t kPeekAddr = 0xc32e;
constexpr uint16_t kPeekValue = 0xc32e;
constexpr unsigned int kPeekTimeoutMilliseconds = 1000;

}  // namespace

namespace mimo_monitor {

DisplaylinkMonitor::DisplaylinkMonitor() {
  serial_number_port_ = 0;
  displaylink_type_ = 0;
  serial_ = "";
  display_alive_ = false;
}

bool DisplaylinkMonitor::IsDisplaylink(libusb_device *device,
                                       const libusb_device_descriptor &desc) {
  int ret = 0;

  // Check DisplayLink VID and PID.
  if (desc.idVendor != kIdVendor || (desc.idProduct != kIdProductCapture &&
                                     desc.idProduct != kIdProductVue)) {
    return false;
  }

  // Read Manufacturer (should always read DisplayLink).
  ret = ReadID(device, desc.iManufacturer, nullptr);
  if (ret < 0) return false;

  // Read Mimo product.
  ret = ReadID(device, desc.iProduct, nullptr);
  if (ret < 0) return false;

  // Read Mimo Serial Number.
  serial_number_port_ = desc.iSerialNumber;
  ret = ReadID(device, serial_number_port_, &serial_);
  if (ret < 0) return false;

  // Only 1 way to talk to DL125. Can not read FrameBuffer.
  if (desc.idProduct == kIdProductVue)
    displaylink_type_ = 0;
  else
    displaylink_type_ = 1;

  display_alive_ = true;
  return true;
}

bool DisplaylinkMonitor::ReadFrameBuffer(libusb_device *device) {
  int ret = 0;
  int frame_cnt1 = 0;
  int frame_cnt2 = 0;
  int frame_diff = 0;
  bool status_alive = true;
  unsigned char data_buf[16] = {0};
  libusb_device_handle *raw_dev_handle = NULL;

  if (libusb_open(device, &raw_dev_handle) < 0) {
    LOG(WARNING) << "Failed to open displaylink device.";
    return false;
  }

  ScopedUsbDevHandle dev_handle(raw_dev_handle);

  if ((libusb_kernel_driver_active(dev_handle.get(), displaylink_type_)) == 1) {
    ret = libusb_detach_kernel_driver(dev_handle.get(), displaylink_type_);
    if (ret < 0) {
      LOG(WARNING) << "Detach kernel driver fail, error: " << ret;
      return false;
    }
  }

  ret = libusb_claim_interface(dev_handle.get(), displaylink_type_);

  if (ret < 0) {
    LOG(WARNING) << "Claim DisplayLink interface fail, error " << ret;
    return false;
  }

  // Read frame count.
  ret = Peek(dev_handle.get(), kPeekValue, kPeekAddr, data_buf, 1);

  // Frame count is only an 8 bit rolling counter.
  frame_cnt1 = data_buf[0];

  // Let a few frames go by. Use loop to avoid early return here.
  base::PlatformThread::Sleep(base::Milliseconds(50));

  // Read frame count again.
  ret = Peek(dev_handle.get(), kPeekValue, kPeekAddr, data_buf, 1);
  frame_cnt2 = data_buf[0];

  // Number of frames that went by.
  frame_diff = frame_cnt2 - frame_cnt1;

  // Fix 8 bit roll over.
  if (frame_diff < 0) frame_diff += 256;
  VLOG(1) << "Read out frame count " << frame_diff;

  // Usually the frame rate is 60 fps and it should nevert go as high as 240
  // fps.
  if ((frame_diff > 12) || (frame_diff < 2)) status_alive = false;

  ret = libusb_release_interface(dev_handle.get(), displaylink_type_);
  if (ret < 0) {
    LOG(WARNING) << "Release interface fail, error " << ret;
    return false;
  }

  ret = libusb_attach_kernel_driver(dev_handle.get(), displaylink_type_);
  if (ret < 0) {
    LOG(WARNING) << "Attach kernel driver fail, error " << ret;
    return false;
  }
  return status_alive;
}

int DisplaylinkMonitor::Peek(libusb_device_handle *dev_handle, int value,
                             int addr, unsigned char *data, int data_len) {
  return libusb_control_transfer(
      dev_handle, LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN, 0x4, value,
      addr, data, data_len, kPeekTimeoutMilliseconds);
}

void DisplaylinkMonitor::CheckDLHealth(libusb_device *device) {
  int ret = 0;
  std::string info;

  if (UseDeepPing(displaylink_type_)) {
    // Read FrameBuffer to ensure it is increasing. Only works on PID 416D.
    display_alive_ = ReadFrameBuffer(device);
  } else {
    // Read Mimo Serial Number from the display portion
    ret = ReadID(device, serial_number_port_, &info);

    if (ret < 0) {
      // In this case, there was no response from the display at all
      LOG(WARNING) << "Original SN: " << info << " Readback error: " << ret;
      display_alive_ = false;
      return;
    }

    // SN Does not match.
    if (serial_ != info) {
      LOG(WARNING) << "Serial number mismatch, original: " << serial_
                   << " new: " << info;
      display_alive_ = false;
    } else {
      display_alive_ = true;
    }
  }
}

bool DisplaylinkMonitor::DisplayStatus() { return display_alive_; }

}  // namespace mimo_monitor
