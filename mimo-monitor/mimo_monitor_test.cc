// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/time/time.h>
#include <brillo/test_helpers.h>

#include "cfm-device-monitor/mimo-monitor/mimo_monitor.h"

namespace mimo_monitor {

class MimoMonitorTest : public testing::Test {
 public:
  MimoMonitorTest() {}
  MimoMonitorTest(const MimoMonitorTest&) = delete;
  MimoMonitorTest& operator=(const MimoMonitorTest&) = delete;

  ~MimoMonitorTest() override = default;
};

TEST_F(MimoMonitorTest, TestResetThrottle) {
    auto mimo_monitor = mimo_monitor::MimoMonitor::Create();

    // up to three times in ten minutes
    time_t start = 1722375720;
    time_t attempts[10] = {0, 100, 300, 599, 601,
                           699, 701, 899, 901, 1200 };  // seconds
    bool allowed[10] = {true, true, true, false, true,
                        false, true, false, true, false };  // expected

    for (int i = 0; i < 10; ++i) {
        base::Time reset_time = base::Time::FromTimeT(start + attempts[i]);
        EXPECT_EQ(mimo_monitor->AllowReset(reset_time), allowed[i]);
    }
}

}  // namespace mimo_monitor

int main(int argc, char** argv) {
  SetUpTests(&argc, argv, true);
  return RUN_ALL_TESTS();
}
