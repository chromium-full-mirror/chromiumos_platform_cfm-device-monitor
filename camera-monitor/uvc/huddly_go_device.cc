// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/camera-monitor/uvc/huddly_go_device.h"

#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <endian.h>
#include <sys/types.h>
#include <vector>

namespace cfm {
namespace uvc {

namespace {
const uint8_t kHuddlyPowerMonitorDataLen = 36;
const uint8_t kHuddlyPowerMonitorModeValLen = 9;
const uint8_t kHuddlyTemperatureMonitorDataLen = 24;
const uint8_t kHuddlyTemperatureMonitorModeValLen = 9;
}  // namespace

HuddlyGoDevice::HuddlyGoDevice(uint16_t vendor_id, uint16_t product_id)
  : VideoDevice(vendor_id, product_id) {
  SetUnitId(kHuddlyUnitId);
}

HuddlyGoDevice::~HuddlyGoDevice() {
  if (IsValid()) {
    CloseDevice();
  }
}

int HuddlyGoDevice::GetCameraMode(HuddlyStreamMode* mode) {
  std::vector<unsigned char> raw_data;

  if (!GetControl(HuddlyXuControl::CAMERA_MODE, &raw_data)) {
    return -1;
  }

  uint16_t mode_val = (raw_data[1] << 8) | raw_data[0];

  *mode = static_cast<HuddlyStreamMode>(le16toh(mode_val));

  return 0;
}

int HuddlyGoDevice::GetTemperature(HuddlyTemperatureMonitor* monitor) {
  std::vector<unsigned char> raw_data;

  if (!GetControl(HuddlyXuControl::TEMPERATURE_MONITOR, &raw_data)) {
    return -1;
  }

  uint32_t mode_val[kHuddlyTemperatureMonitorModeValLen];

  if (raw_data.size() != kHuddlyTemperatureMonitorDataLen) {
    return -1;  // did not return correct size
  }

  for (int i = 0; i < kHuddlyTemperatureMonitorDataLen; i = i + 4) {
    uint32_t val = (raw_data[i + 3] << 24) | (raw_data[i + 2] << 16) |
        (raw_data[i + 1] << 8) | raw_data[i];
    mode_val[i / 4] = le32toh(val);
  }

  *monitor = *reinterpret_cast<HuddlyTemperatureMonitor*>(&mode_val[0]);

  return 0;
}

int HuddlyGoDevice::GetPower(HuddlyPowerMonitor* monitor) {
  std::vector<unsigned char> raw_data;

  if (!GetControl(HuddlyXuControl::POWER_MONITOR, &raw_data)) {
    return -1;
  }

  uint32_t mode_val[kHuddlyPowerMonitorModeValLen];

  if (raw_data.size() != kHuddlyPowerMonitorDataLen) {
    return -1;  // did not return correct size
  }

  for (int i = 0; i < kHuddlyPowerMonitorDataLen; i = i + 4) {
    uint32_t val = (raw_data[i + 3] << 24) | (raw_data[i + 2] << 16) |
        (raw_data[i + 1] << 8) | raw_data[i];
    mode_val[i / 4] = le32toh(val);
  }

  *monitor = *reinterpret_cast<HuddlyPowerMonitor*>(&mode_val[0]);

  return 0;
}

int HuddlyGoDevice::GetVersion(HuddlyVersion* firmware_version) {
  std::vector<unsigned char> version;
  if (!GetControl(HuddlyXuControl::VERSION, &version)) {
    return -1;
  }

  if (version.empty()) {
    return -1;
  }
  firmware_version->bootloader_version =
      base::StringPrintf("%d.%d.%d", version[7], version[6], version[5]);
  firmware_version->application_version =
      base::StringPrintf("%d.%d.%d", version[3], version[2], version[1]);

  return 0;
}

bool HuddlyGoDevice::Reset() {
  if (!IsValid()) {
    OpenDevice();
  }
  std::vector<unsigned char> data(2);
  data[0] = HuddlyBootloader::ENABLE & 0xff;
  data[1] = (HuddlyBootloader::ENABLE >> 8);
  if (!SetControl(HuddlyXuControl::REBOOT, data)) {
    LOG(ERROR) << "Unable to enable bootloader for reboot.";
    return false;
  }
  LOG(INFO) << "Enter bootloader for reboot";
  data[0] = HuddlyBootloader::REBOOT_WITHOUT_BOOTLOADER & 0xff;
  data[1] = (HuddlyBootloader::REBOOT_WITHOUT_BOOTLOADER >> 8);
  if (!SetControl(HuddlyXuControl::REBOOT, data)) {
    LOG(ERROR) << "Unable to reboot without bootloader.";
    return false;
  }
  LOG(INFO) << "Reboot without bootloader";
  return true;
}

bool HuddlyGoDevice::GetControl(HuddlyXuControl control_selector,
                               std::vector<unsigned char>* data) {
  if (!IsValid()) {
    OpenDevice();
  }
  const auto selector = static_cast<unsigned char>(control_selector);
  return GetXuControl(selector, data);
}

bool HuddlyGoDevice::SetControl(HuddlyXuControl control_selector,
                               std::vector<unsigned char> data) {
  if (!IsValid()) {
    OpenDevice();
  }
  const auto selector = static_cast<unsigned char>(control_selector);
  return SetXuControl(selector, data);
}

std::string HuddlyGoDevice::StreamModeToStr(HuddlyStreamMode stream_mode) {
  switch (stream_mode) {
    case HuddlyStreamMode::SINGLE:
      return "Single";
    case HuddlyStreamMode::DUAL:
      return "Dual";
    case HuddlyStreamMode::TRIPLE:
      return "Triple";
    case HuddlyStreamMode::WRITE_ENABLE:
      return "Write Enable";
    default:
      uint16_t unknown_val = static_cast<uint16_t>(stream_mode);
      char return_string[15];
      snprintf(return_string, sizeof(return_string), "UNKNOWN:0x%04x",
               unknown_val);
      return return_string;
  }
}

}  // namespace uvc
}  // namespace cfm
