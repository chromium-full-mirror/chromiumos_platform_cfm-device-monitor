// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAMERA_MONITOR_UVC_HUDDLY_GO_DEVICE_H_
#define CAMERA_MONITOR_UVC_HUDDLY_GO_DEVICE_H_

#include <string>
#include <vector>

#include "cfm-device-monitor/camera-monitor/uvc/video_device.h"

namespace cfm {
namespace uvc {

// Huddly specific uvc control extension units (XUs)
enum class HuddlyXuControl : unsigned char {
  CAMERA_MODE = 0x01,
  TEMPERATURE_MONITOR = 0x02,
  POWER_MONITOR = 0x03,
  VERSION = 0x13,
  REBOOT = 0x11,
};

enum class HuddlyStreamMode : uint16_t {
  SINGLE = 0x0000,
  DUAL = 0x0001,
  TRIPLE = 0x0002,
  WRITE_ENABLE = 0x8eb0,
};

enum HuddlyBootloader: uint16_t {
  DISABLE = 0x0000,
  ENABLE = 0x3974,
  REBOOT_WITHOUT_BOOTLOADER = 0x0003,  // Bootloader must be enabled
  REBOOT_WITH_BOOTLOADER = 0x1399,  // Bootloader must be enabled
};

// Huddly temperature data
// Note: Order is important
struct HuddlyTemperatureMonitor {
  float mv2;      // Current sampled temperature internally on MV2 chip (deg C)
  float mv2_min;  // Minimum sampled temperature internally on MV2 chip (deg C)
  float mv2_max;  // Maximum sampled temperature internally on MV2 chip (deg C)
  float pcb;      // Current sampled temperature on PCB (deg C)
  float pcb_min;  // Minimum sampled temperature on PCB (deg C)
  float pcb_max;  // Maximum sampled temperature on PCB (deg C)
} __attribute__((packed));

// Huddly power data
// Note: Order is important
struct HuddlyPowerMonitor {
  float voltage_min;  // Minimum sampled voltage since boot (V)
  float voltage;      // Previous sampled voltage (V)
  float voltage_max;  // Maximum sampled voltage since boot (V)
  float current_min;  // Minimum sampled input current since boot (A)
  float current;      // Previous sampled input current (A)
  float current_max;  // Maximum sampled input current since boot (A)
  float power_min;    // Minimum sampled power usage (W)
  float power;        // Previous sampled power usage (W)
  float power_max;    // Maximum sampled power usage (W)
} __attribute__((packed));

struct HuddlyVersion {
  std::string bootloader_version;   // Current Huddly Bootloader Version
  std::string application_version;  // Current Huddly Firmware Version
};

constexpr uint16_t kHuddlyVendorId = 0x2bd9;
constexpr uint16_t kHuddlyProductId = 0x0011;
constexpr uint8_t kHuddlyUnitId = 4;

class HuddlyGoDevice : public cfm::uvc::VideoDevice {
 public:
  HuddlyGoDevice(uint16_t vendor_id, uint16_t product_id);
  HuddlyGoDevice(const HuddlyGoDevice&) = delete;
  HuddlyGoDevice& operator=(const HuddlyGoDevice&) = delete;

  ~HuddlyGoDevice() override;

  int GetCameraMode(HuddlyStreamMode* mode);
  int GetTemperature(HuddlyTemperatureMonitor* monitor);
  int GetPower(HuddlyPowerMonitor* monitor);
  int GetVersion(HuddlyVersion* firmware_version);
  bool Reset();

  static std::string StreamModeToStr(HuddlyStreamMode stream_mode);

 protected:
  bool GetControl(HuddlyXuControl control_selector,
                  std::vector<unsigned char>* data);

  bool SetControl(HuddlyXuControl control_selector,
                  std::vector<unsigned char> data);
};  // HuddlyGoDevice

}  // namespace uvc
}  // namespace cfm

#endif  // CAMERA_MONITOR_UVC_HUDDLY_GO_DEVICE_H_
