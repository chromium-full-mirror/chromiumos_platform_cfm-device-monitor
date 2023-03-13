// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAMERA_MONITOR_UDEV_H_
#define CAMERA_MONITOR_UDEV_H_

#include <libudev.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"

#include "cfm-device-monitor/camera-monitor/tools.h"

namespace huddly_monitor {

// Simple wrapper around udev, tracking if huddly camera is connected or not.
class Udev {
 public:
  using DeviceCallback = base::RepeatingCallback<void()>;

  Udev(const DeviceCallback& camera_added_callback,
       const DeviceCallback& camera_removed_callback);
  Udev(const Udev&) = delete;
  Udev& operator=(const Udev&) = delete;

  ~Udev();

  // Returns true if a huddly camera is currently connected.
  bool IsCameraConnected() const;

 private:
  struct UdevDeleter {
    void operator()(udev* udev) const;
  };

  struct UdevMonitorDeleter {
    void operator()(udev_monitor*) const;
  };

  const std::string kCameraVid = base::StringPrintf("%04x", kHuddlyVid);
  const std::string kCameraPid = base::StringPrintf("%04x", kHuddlyPid);

  // Callback receiving events from udev.
  void OnDeviceAction();

  DeviceCallback camera_added_callback_;
  DeviceCallback camera_removed_callback_;

  std::unique_ptr<udev, UdevDeleter> udev_;
  std::unique_ptr<udev_monitor, UdevMonitorDeleter> monitor_;

  std::unique_ptr<base::FileDescriptorWatcher::Controller> watcher_;

  base::WeakPtrFactory<Udev> weak_factory_{this};
};

}  // namespace huddly_monitor

#endif  // CAMERA_MONITOR_UDEV_H_
