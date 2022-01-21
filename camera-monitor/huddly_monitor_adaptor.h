// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAMERA_MONITOR_HUDDLY_MONITOR_ADAPTOR_H_
#define CAMERA_MONITOR_HUDDLY_MONITOR_ADAPTOR_H_
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "brillo/any.h"
#include "brillo/dbus/dbus_object.h"
#include "brillo/dbus/exported_object_manager.h"
#include "brillo/variant_dictionary.h"
#include "dbus/object_path.h"

namespace org {
namespace chromium {

// Interface definition for org::chromium::huddlymonitor.
class huddlymonitorInterface {
 public:
  virtual ~huddlymonitorInterface() = default;

  // Updates the condition of the huddly monitor.
  virtual bool UpdateCondition(brillo::ErrorPtr *error, bool in_condition,
                               uint64_t in_timestamp) = 0;
};

// Interface adaptor for org::chromium::huddlymonitor.
class huddlymonitorAdaptor {
 public:
  huddlymonitorAdaptor(const huddlymonitorAdaptor&) = delete;
  huddlymonitorAdaptor& operator=(const huddlymonitorAdaptor&) = delete;

  explicit huddlymonitorAdaptor(huddlymonitorInterface *interface)
      : interface_(interface) {}

  void RegisterWithDBusObject(brillo::dbus_utils::DBusObject *object) {
    brillo::dbus_utils::DBusInterface *itf =
        object->AddOrGetInterface("org.chromium.huddlymonitor");

    itf->AddSimpleMethodHandlerWithError(
        "UpdateCondition", base::Unretained(interface_),
        &huddlymonitorInterface::UpdateCondition);
  }

  static dbus::ObjectPath GetObjectPath() {
    return dbus::ObjectPath{"/org/chromium/huddlymonitor"};
  }

 private:
  huddlymonitorInterface *interface_;  // Owned by container of this adapter.
};

}  // namespace chromium
}  // namespace org

#endif  // CAMERA_MONITOR_HUDDLY_MONITOR_ADAPTOR_H_
