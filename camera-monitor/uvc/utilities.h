// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAMERA_MONITOR_UVC_UTILITIES_H_
#define CAMERA_MONITOR_UVC_UTILITIES_H_

#include <base/files/file_path.h>
#include <cstdio>
#include <string>
#include <vector>

namespace cfm {
namespace uvc {

/**
 * Get contents of the given directory.
 */
bool GetDirectoryContents(const std::string& directory,
                          std::vector<std::string>* contents);
/*
 * Iterate the mount point to look for the given vid and pid.
 * Important to note that device_point must be a relative path
 */
bool FindDevices(std::string mount_point, std::string device_point,
                 base::FilePath dev_dir, uint16_t usb_vid, uint16_t usb_pid,
                 std::vector<base::FilePath> *devices);

}  // namespace uvc
}  // namespace cfm

#endif  // CAMERA_MONITOR_UVC_UTILITIES_H_
