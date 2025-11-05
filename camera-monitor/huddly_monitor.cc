// Copyright 2017 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/camera-monitor/huddly_monitor.h"

#include <base/functional/bind.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/system/sys_info.h>
#include <base/task/single_thread_task_runner.h>
#include <base/threading/platform_thread.h>
#include <base/time/time.h>
#include <dbus/message.h>
#include <dbus/permission_broker/dbus-constants.h>

#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "cfm-device-monitor/camera-monitor/tools.h"
#include "cfm-device-monitor/camera-monitor/uvc/huddly_go_device.h"

using permission_broker::kPermissionBrokerInterface;
using permission_broker::kPermissionBrokerServiceName;
using permission_broker::kPermissionBrokerServicePath;
using permission_broker::kPowerCycleUsbPorts;

namespace huddly_monitor {

namespace {

const char kError[] = "uvcvideo: Failed";
const char kErrorException[] = "uvcvideo: Failed to resubmit video URB";
const float kMaxCurrent = 0.75f;
const float kMaxVoltage = 5.25f;
const float kMinVoltage = 4.45f;
const float kMaxChipTemperature = 110.0f;
const float kMaxBoardTemperature = 85.0f;
}  // namespace

// TODO(felixe): Using popen() to get a FILE* here and then using fileno() is
// not safe. Rewrite this to use base/process/launch.h.
HuddlyMonitor::HuddlyMonitor(bool init_wait_val, uint32_t sleep_time)
    : AbstractMonitor(init_wait_val, sleep_time),
      klog_pipe_(popen("dmesg -w --level=err", "r")), error_matcher_(kError),
      error_exception_(kErrorException) {
  camera_ = std::make_unique<cfm::uvc::HuddlyGoDevice>(
      cfm::uvc::kHuddlyVendorId, cfm::uvc::kHuddlyProductId);
}

HuddlyMonitor::~HuddlyMonitor() {
  pclose(klog_pipe_);
  camera_->CloseDevice();
}

bool HuddlyMonitor::VitalsExist() {
  std::string msg = "";

  bool found_error =
      LookForErrorBlocking(error_matcher_, error_exception_, klog_pipe_, &msg);

  if (!msg.empty()) {
    LOG(ERROR) << "Failed trying to monitor camera. " << msg;
  }

  return !found_error;
}

bool HuddlyMonitor::InitDBus() {
  if (!bus_) {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = new dbus::Bus(std::move(options));
  }
  if (!bus_->Connect()) {
    LOG(ERROR) << "Failed to connect to D-Bus";
    return false;
  }
  permission_broker_proxy_ =
      bus_->GetObjectProxy(kPermissionBrokerServiceName,
                           dbus::ObjectPath(kPermissionBrokerServicePath));
  if (!permission_broker_proxy_) {
    LOG(ERROR) << "Failed to get D-Bus object proxy for permission_broker";
    return false;
  }
  return true;
}

void HuddlyMonitor::CheckMaxThreshold(const std::string& property,
                                      float value, float threshold) {
  if (value >= threshold) {
    LOG(WARNING) << "Reached max " << property << " threshold ("
                 << threshold << "): " << value;
  }
}

void HuddlyMonitor::CheckMinThreshold(const std::string& property,
                                      float value, float threshold) {
  if (value <= threshold) {
    LOG(WARNING) << "Reached min " << property << " threshold ("
                 << threshold << "): " << value;
  }
}

void HuddlyMonitor::StartMetricsLog(const base::TimeDelta &start_delay,
                                    const base::TimeDelta &time_interval) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&HuddlyMonitor::LogPeriodically, base::Unretained(this),
                     time_interval),
      start_delay);
}

void HuddlyMonitor::LogPeriodically(const base::TimeDelta& time_interval) {
  camera_->OpenDevice();
  if (camera_->IsValid()) {
    LogCameraInfo();
    LogTemperature();
    LogPower();
  }
  camera_->CloseDevice();

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&HuddlyMonitor::LogPeriodically, base::Unretained(this),
                     time_interval),
      time_interval);
}

void HuddlyMonitor::LogTemperature() {
  cfm::uvc::HuddlyTemperatureMonitor temperature_monitor;
  int ret = camera_->GetTemperature(&temperature_monitor);
  if (ret < 0) {
    LOG(ERROR) << "Failed to get temperature.";
    return;
  }
  LOG(INFO) << "Min temperature on MV2 chip:" << temperature_monitor.mv2_min;
  LOG(INFO) << "Current temperature on MV2 chip:" << temperature_monitor.mv2;
  LOG(INFO) << "Max temperature on MV2 chip:" << temperature_monitor.mv2_max;
  CheckMaxThreshold("chip temperature", temperature_monitor.mv2_max,
                    kMaxChipTemperature);
  LOG(INFO) << "Min temperature on PCB:" << temperature_monitor.pcb_min;
  LOG(INFO) << "Current temperature on PCB:" << temperature_monitor.pcb;
  LOG(INFO) << "Max temperature on PCB:" << temperature_monitor.pcb_max;
  CheckMaxThreshold("board temperature", temperature_monitor.pcb_max,
                    kMaxBoardTemperature);
}

void HuddlyMonitor::LogPower() {
  cfm::uvc::HuddlyPowerMonitor power_monitor;
  int ret = camera_->GetPower(&power_monitor);
  if (ret < 0) {
    LOG(ERROR) << "Failed to get power.";
    return;
  }
  LOG(INFO) << "Min voltage:" << power_monitor.voltage_min;
  CheckMinThreshold("voltage", power_monitor.voltage_min, kMinVoltage);
  LOG(INFO) << "Previous sampled voltage:" << power_monitor.voltage;
  LOG(INFO) << "Max voltage:" << power_monitor.voltage_max;
  CheckMaxThreshold("voltage", power_monitor.voltage_max, kMaxVoltage);
  LOG(INFO) << "Min current:" << power_monitor.current_min;
  LOG(INFO) << "Previous sampled current:" << power_monitor.current;
  LOG(INFO) << "Max current:" << power_monitor.current_max;
  CheckMaxThreshold("current", power_monitor.current_max, kMaxCurrent);
  LOG(INFO) << "Min power usage:" << power_monitor.power_min;
  LOG(INFO) << "Previously sampled power usage:" << power_monitor.power;
  LOG(INFO) << "Max power usage:" << power_monitor.power_max;
}

void HuddlyMonitor::LogCameraInfo() {
  cfm::uvc::HuddlyVersion version;
  int ret = camera_->GetVersion(&version);
  if (ret < 0) {
    LOG(ERROR) << "Failed to get version.";
    return;
  }
  cfm::uvc::HuddlyStreamMode stream_mode;
  int ret_t = camera_->GetCameraMode(&stream_mode);
  if (ret_t < 0) {
    LOG(ERROR) << "Failed to get stream mode.";
    return;
  }
  LOG(INFO) << "Bootloader version:" << version.bootloader_version;
  LOG(INFO) << "Application version:" << version.application_version;
  LOG(INFO) << "Stream mode:"
            << cfm::uvc::HuddlyGoDevice::StreamModeToStr(stream_mode);
}

bool HuddlyMonitor::PowerCycleUsbPort(uint16_t vid, uint16_t pid,
                                      base::TimeDelta delay) {
  if (!permission_broker_proxy_ && !InitDBus()) {
    return false;
  }

  dbus::MethodCall method_call(kPermissionBrokerInterface, kPowerCycleUsbPorts);
  dbus::MessageWriter writer(&method_call);
  writer.AppendUint16(vid);
  writer.AppendUint16(pid);
  writer.AppendInt64(delay.ToInternalValue());

  return permission_broker_proxy_->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT).has_value();
}

bool HuddlyMonitor::Respond() {
  bool result = false;

  if (camera_->IsValid()) {
    LOG(INFO) << "Soft reset through XU control.";
    result = camera_->Reset();
  } else {
    const base::TimeDelta kPowerCycleDelay =
      base::Milliseconds(200);
    // Guado can't power cycle USB devices trough permission_broker, it uses a
    // board specific implementation instead.
    std::string board = base::SysInfo::GetLsbReleaseBoard();
    if (base::StartsWith(board, "guado", base::CompareCase::SENSITIVE)) {
      LOG(INFO) << "Hard reset through hotplugging guado.";
      result = HotplugDeviceGuado(kHuddlyVid, kHuddlyPid, klog_pipe_);
    } else if (base::StartsWith(board, "fizz", base::CompareCase::SENSITIVE)) {
      LOG(INFO) << "Hard reset through power cycling port for fizz.";
      result = PowerCycleUsbPort(kHuddlyVid, kHuddlyPid, kPowerCycleDelay);
      ConsumeAndDiscardInput(klog_pipe_);
    } else {
      LOG(WARNING) << "Unrecognized board type";
      result = false;
    }
  }

  if (result) {
    // Please see b/109866345 before modyfing the below line.
    LOG(WARNING) << "Detected crashed camera. Rebooted.";
    const uint32_t kRebootSleepTimeSeconds = 30;
    base::PlatformThread::Sleep(
        base::Seconds(kRebootSleepTimeSeconds));
  }

  return result;
}

}  // namespace huddly_monitor
