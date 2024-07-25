// Copyright 2017 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAMERA_MONITOR_HUDDLY_MONITOR_H_
#define CAMERA_MONITOR_HUDDLY_MONITOR_H_

#include <libusb-1.0/libusb.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#include <base/time/time.h>
#include <dbus/bus.h>
#include <dbus/object_proxy.h>

#include <memory>
#include <string>
#include <vector>

#include "cfm-device-monitor/camera-monitor/abstract_monitor.h"
#include "cfm-device-monitor/camera-monitor/uvc/huddly_go_device.h"

namespace huddly_monitor {

class HuddlyMonitor : public AbstractMonitor {
 public:
  HuddlyMonitor(bool init_wait_val, uint32_t sleep_time);
  HuddlyMonitor(const HuddlyMonitor&) = delete;
  HuddlyMonitor& operator=(const HuddlyMonitor&) = delete;

  ~HuddlyMonitor();

  void StartMetricsLog(const base::TimeDelta &start_delay,
                       const base::TimeDelta &time_interval) override;
  bool VitalsExist() override;
  bool Respond() override;

 private:
  bool InitDBus();
  bool PowerCycleUsbPort(uint16_t vid, uint16_t pid, base::TimeDelta delay);
  void CheckMaxThreshold(const std::string& property,
                         float value, float threshold);
  void CheckMinThreshold(const std::string& property,
                         float value, float threshold);

  void LogPeriodically(const base::TimeDelta &time_interval);
  void LogPower();
  void LogTemperature();
  void LogCameraInfo();

  FILE *klog_pipe_;
  const std::string error_matcher_;
  const std::string error_exception_;
  scoped_refptr<dbus::Bus> bus_ = nullptr;
  dbus::ObjectProxy* permission_broker_proxy_ = nullptr;
  std::unique_ptr<cfm::uvc::HuddlyGoDevice> camera_ = nullptr;
};

}  // namespace huddly_monitor

#endif  // CAMERA_MONITOR_HUDDLY_MONITOR_H_
