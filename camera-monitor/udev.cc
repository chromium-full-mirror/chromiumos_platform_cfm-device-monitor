// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/camera-monitor/udev.h"

#include <base/functional/bind.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>

namespace huddly_monitor {

namespace {

struct UdevDeviceDeleter {
  void operator()(udev_device* device) const { udev_device_unref(device); }
};

struct UdevEnumerateDeleter {
  void operator()(udev_enumerate* enumerate) const {
    udev_enumerate_unref(enumerate);
  }
};

}  // namespace

Udev::Udev(const DeviceCallback& camera_added_callback,
           const DeviceCallback& camera_removed_callback) {
  camera_added_callback_ = camera_added_callback;
  camera_removed_callback_ = camera_removed_callback;

  udev_.reset(udev_new());
  LOG_IF(FATAL, !udev_) << "Failed to create libudev instance";

  monitor_.reset(udev_monitor_new_from_netlink(udev_.get(), "udev"));
  LOG_IF(FATAL, !monitor_) << "Failed to create udev monitor";

  if (udev_monitor_filter_add_match_subsystem_devtype(monitor_.get(), "usb",
                                                      nullptr) < 0) {
    LOG(FATAL) << "Failed to create udev filter for cec devices";
  }

  if (udev_monitor_enable_receiving(monitor_.get()) < 0) {
    LOG(FATAL) << "Failed to enable receiving on udev monitor";
  }

  watcher_ = base::FileDescriptorWatcher::WatchReadable(
      udev_monitor_get_fd(monitor_.get()),
      base::BindRepeating(&Udev::OnDeviceAction, weak_factory_.GetWeakPtr()));
  if (watcher_ == nullptr) {
    LOG(FATAL) << "Failed to register listener on udev descriptor";
  }
}

Udev::~Udev() = default;

bool Udev::IsCameraConnected() const {
  std::unique_ptr<udev_enumerate, UdevEnumerateDeleter> enumerate(
      udev_enumerate_new(udev_.get()));
  if (!enumerate) {
    LOG(ERROR) << "Failed to create udev enumeration";
  }

  if (udev_enumerate_add_match_subsystem(enumerate.get(), "usb") < 0) {
    LOG(FATAL) << "Failed to add subsytem filter to udev enumeration";
  }

  if (udev_enumerate_add_match_property(enumerate.get(), "ID_VENDOR_ID",
                                        kCameraVid.c_str()) < 0) {
    LOG(FATAL) << "Failed to add vendor id filter to udev enumeration";
  }

  if (udev_enumerate_add_match_property(enumerate.get(), "ID_MODEL_ID",
                                        kCameraPid.c_str()) < 0) {
    LOG(FATAL) << "Failed to add product id filter to udev enumeration";
  }

  if (udev_enumerate_scan_devices(enumerate.get()) < 0) {
    LOG(FATAL) << "Failed to scan devices with udev";
  }

  udev_list_entry* devices_list =
      udev_enumerate_get_list_entry(enumerate.get());
  return devices_list != nullptr;
}

void Udev::OnDeviceAction() {
  std::unique_ptr<udev_device, UdevDeviceDeleter> device(
      udev_monitor_receive_device(monitor_.get()));
  if (!device) {
    return;
  }

  const char* vid =
      udev_device_get_property_value(device.get(), "ID_VENDOR_ID");
  if (!vid || kCameraVid != vid) {
    return;
  }

  const char* pid = udev_device_get_property_value(device.get(), "ID_MODEL_ID");
  if (!pid || kCameraPid != pid) {
    return;
  }

  const char* action = udev_device_get_action(device.get());
  if (!action) {
    LOG(WARNING) << "Failed to get device action";
    return;
  }

  if (!strcmp(action, "add")) {
    camera_added_callback_.Run();
  } else if (!strcmp(action, "remove")) {
    camera_removed_callback_.Run();
  }
}

void Udev::UdevDeleter::operator()(udev* udev) const { udev_unref(udev); }

void Udev::UdevMonitorDeleter::operator()(udev_monitor* udev) const {
  udev_monitor_unref(udev);
}

}  // namespace huddly_monitor
