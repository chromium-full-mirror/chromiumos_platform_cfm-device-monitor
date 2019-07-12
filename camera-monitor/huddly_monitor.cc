// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/camera-monitor/huddly_monitor.h"

#include <base/bind.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/strings/string_util.h>
#include <base/sys_info.h>
#include <base/threading/platform_thread.h>
#include <base/time/time.h>
#include <dbus/message.h>
#include <dbus/permission_broker/dbus-constants.h>
#include <dbus/scoped_dbus_error.h>

#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <string>

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
    bus_ = new dbus::Bus(options);
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

void HuddlyMonitor::LogPeriodically(const base::TimeDelta& time_interval) {
  LogCameraInfo();
  LogTemperature();
  LogPower();
  base::MessageLoop::current()->task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&HuddlyMonitor::LogPeriodically,
                            base::Unretained(this), time_interval),
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
  LOG(INFO) << "Min temperature on PCB:" << temperature_monitor.pcb_min;
  LOG(INFO) << "Current temperature on PCB:" << temperature_monitor.pcb;
  LOG(INFO) << "Max temperature on PCB:" << temperature_monitor.pcb_max;
}

void HuddlyMonitor::LogPower() {
  cfm::uvc::HuddlyPowerMonitor power_monitor;
  int ret = camera_->GetPower(&power_monitor);
  if (ret < 0) {
    LOG(ERROR) << "Failed to get power.";
    return;
  }
  LOG(INFO) << "Min voltage:" << power_monitor.voltage_min;
  LOG(INFO) << "Previous sampled voltage:" << power_monitor.voltage;
  LOG(INFO) << "Max voltage:" << power_monitor.voltage_max;
  LOG(INFO) << "Min current:" << power_monitor.current_min;
  LOG(INFO) << "Previous sampled current:" << power_monitor.current;
  LOG(INFO) << "Max current:" << power_monitor.current_max;
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
  std::unique_ptr<dbus::Response> response(
      permission_broker_proxy_->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  return response.get();
}

bool HuddlyMonitor::Respond() {
  const base::TimeDelta kPowerCycleDelay =
      base::TimeDelta::FromMilliseconds(200);

  bool result = false;

  // Guado can't power cycle USB devices trough permission_broker, it uses a
  // board specific implementation instead.
  std::string board = base::SysInfo::GetLsbReleaseBoard();
  if (base::StartsWith(board, "guado", base::CompareCase::SENSITIVE)) {
    result = HotplugDeviceGuado(kHuddlyVid, kHuddlyPid, klog_pipe_);
  } else if (base::StartsWith(board, "fizz", base::CompareCase::SENSITIVE)) {
    result = PowerCycleUsbPort(kHuddlyVid, kHuddlyPid, kPowerCycleDelay);
    ConsumeAndDiscardInput(klog_pipe_);
  } else {
    LOG(WARNING) << "Unrecognized board type";
    result = false;
  }

  if (result) {
    // Please see b/109866345 before modyfing the below line.
    LOG(WARNING) << "Detected crashed camera. Rebooted.";
    const uint32_t kRebootSleepTimeSeconds = 30;
    base::PlatformThread::Sleep(
        base::TimeDelta::FromSeconds(kRebootSleepTimeSeconds));
  }

  return result;
}

}  // namespace huddly_monitor
