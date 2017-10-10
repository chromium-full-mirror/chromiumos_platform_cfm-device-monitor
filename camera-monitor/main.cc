// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <getopt.h>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "base/logging.h"
#include "brillo/syslog_logging.h"
#include "brillo/daemons/dbus_daemon.h"
#include "cfm-device-monitor/camera-monitor/huddly_monitor.h"
#include "cfm-device-monitor/camera-monitor/huddly_monitor_adaptor.h"

namespace huddly_monitor {

const char kHuddlyServicePath[] = "/org/chromium/huddlymonitor";
const char kHuddlyServiceName[] = "org.chromium.huddlymonitor";

}  // namespace huddly_monitor

class DBusAdaptor : public org::chromium::huddlymonitorInterface,
                    public org::chromium::huddlymonitorAdaptor {
 public:
  explicit DBusAdaptor(scoped_refptr<dbus::Bus> bus)
      : org::chromium::huddlymonitorAdaptor(this),
        dbus_object_(nullptr, bus,
                     dbus::ObjectPath(huddly_monitor::kHuddlyServicePath)),
        monitor_object_(false, 500) {
    monitor_object_.StartMonitor();
  }

  void RegisterAsync(
      const brillo::dbus_utils::AsyncEventSequencer::CompletionAction &cb) {
    RegisterWithDBusObject(&dbus_object_);
    dbus_object_.RegisterAsync(cb);
  }

  bool UpdateCondition(brillo::ErrorPtr *error, bool in_condition,
                       uint64_t in_timestamp) override {
    bool update_made =
        monitor_object_.UpdateCondition(in_condition, in_timestamp);
    if (!update_made) {
      LOG(INFO) << "Duplicate update seen. Dropping.";
    }
    return true;
  }

 private:
  brillo::dbus_utils::DBusObject dbus_object_;
  huddly_monitor::HuddlyMonitor monitor_object_;

  DISALLOW_COPY_AND_ASSIGN(DBusAdaptor);
};

class HuddlyDaemon : public brillo::DBusServiceDaemon {
 public:
  HuddlyDaemon() : DBusServiceDaemon(huddly_monitor::kHuddlyServiceName) {}

 protected:
  void RegisterDBusObjectsAsync(
      brillo::dbus_utils::AsyncEventSequencer *sequencer) override {
    adaptor_.reset(new DBusAdaptor(bus_));
    adaptor_->RegisterAsync(
        sequencer->GetHandler("RegisterAsync() failed", true));
  }

 private:
  std::unique_ptr<DBusAdaptor> adaptor_;

  DISALLOW_COPY_AND_ASSIGN(HuddlyDaemon);
};

int main(int argc, char *argv[]) {
  brillo::InitLog(brillo::InitFlags::kLogToSyslog);
  logging::LoggingSettings logging_settings;
  logging::InitLogging(logging_settings);

  VLOG(1) << "Starting huddly monitor.";
  return HuddlyDaemon().Run();
}
