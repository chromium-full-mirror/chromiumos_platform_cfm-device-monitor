// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cfm-device-monitor/camera-monitor/uvc/utilities.h"

#include <base/files/dir_reader_posix.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>

#include <cstdint>
#include <iomanip>
#include <string>
#include <vector>

namespace cfm {
namespace uvc {

bool GetDirectoryContents(const std::string& directory,
                          std::vector<std::string>* contents) {
  base::DirReaderPosix reader(directory.c_str());
  if (!reader.IsValid()) {
    return false;
  }

  while (reader.Next()) {
    contents->push_back(std::string(reader.name()));
  }
  return true;
}

bool FindDevices(std::string mount_point, std::string device_point,
                 base::FilePath dev_dir, uint16_t usb_vendor_id,
                 uint16_t usb_product_id,
                 std::vector<base::FilePath> *devices) {
  if (mount_point.empty() || device_point.empty()) {
    return false;
  }

  std::vector<std::string> contents;
  if (!GetDirectoryContents(mount_point, &contents)) {
    return false;  // Failed to get contents of mount point.
  }

  for (auto const& content : contents) {
    if (content.compare(".") == 0 || content.compare("..") == 0) {
      continue;  // Ignores . and .. directory.
    }
    base::FilePath device_dir(mount_point + "/" + content + "/");
    // Reads mount point device vendor id & product id.
    base::FilePath product_id_path =
        device_dir.Append(device_point + "/idProduct");
    base::FilePath vendor_id_path =
        device_dir.Append(device_point + "/idVendor");
    std::string vendor_id;
    std::string product_id;

    base::FilePath vendor_true_path =
        base::MakeAbsoluteFilePath(vendor_id_path);
    base::File file(vendor_true_path,
                    base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!file.IsValid()) {
      continue;
    }
    if (!base::ReadFileToString(vendor_true_path, &vendor_id)) {
      LOG(ERROR) << "Failed to read vendor ID";
      continue;
    }
    int vidnum = 0;
    std::string trimmed;
    base::TrimWhitespaceASCII(vendor_id, base::TRIM_ALL, &trimmed);
    if (!base::HexStringToInt(trimmed, &vidnum)) {
      LOG(ERROR) << "Failed to convert vendor ID ("
                 << trimmed << ") to int";
      continue;
    }
    if (vidnum != usb_vendor_id) {
      LOG(ERROR) << "Not the intended vendor";
      continue;
    }
    base::FilePath product_true_path =
        base::MakeAbsoluteFilePath(product_id_path);
    if (!base::ReadFileToString(product_true_path, &product_id)) {
      LOG(ERROR) << "Failed to read product ID";
      continue;
    }
    int pidnum = 0;
    base::TrimWhitespaceASCII(product_id, base::TRIM_ALL, &trimmed);
    if (!base::HexStringToInt(trimmed, &pidnum)) {
      LOG(ERROR) << "Failed to convert product ID to int";
      continue;
    }

    if (pidnum == usb_product_id) {
      LOG(INFO) << "Device path" << dev_dir.Append(content).value();
      devices->push_back(dev_dir.Append(content));
    }
  }
  return true;
}

}  // namespace uvc
}  // namespace cfm
