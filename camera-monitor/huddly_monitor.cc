// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/camera-monitor/huddly_monitor.h"

#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/sys_info.h>
#include <base/time/time.h>
#include <base/threading/platform_thread.h>
#include <dbus/message.h>
#include <dbus/permission_broker/dbus-constants.h>
#include <dbus/scoped_dbus_error.h>

#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "cfm-device-monitor/camera-monitor/tools.h"

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
      klog_pipe_(popen("dmesg -w --level=err", "r")),
      error_matcher_(kError),
      error_exception_(kErrorException) {}

HuddlyMonitor::~HuddlyMonitor() { pclose(klog_pipe_); }

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
    LOG(ERROR) << "Faild to connect to D-Bus";
    return false;
  }
  permission_broker_proxy_ =
      bus_->GetObjectProxy(kPermissionBrokerServiceName,
                           dbus::ObjectPath(kPermissionBrokerServicePath));
  if (!permission_broker_proxy_) {
    LOG(ERROR) << "Faild to get D-Bus object proxy for permission_broker";
    return false;
  }
  return true;
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
  bool result;

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
    LOG(WARNING) << "Detected crashed camera. Rebooted.";
    const uint32_t kRebootSleepTimeSeconds = 30;
    base::PlatformThread::Sleep(
        base::TimeDelta::FromSeconds(kRebootSleepTimeSeconds));
  }

  return result;
}

}  // namespace huddly_monitor
