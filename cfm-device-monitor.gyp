# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libusb-1.0',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libcamera-monitor',
      'type': 'static_library',
      'sources': [
        'camera-monitor/abstract_monitor.cc',
        'camera-monitor/huddly_monitor.cc',
        'camera-monitor/tools.cc',
      ],
    },
    {
      'target_name': 'huddly-monitor',
      'type': 'executable',
      'dependencies': [
        'libcamera-monitor',
      ],
      'sources': [
        'camera-monitor/main.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'camera-monitor-test',
          'type': 'executable',
          'includes': ['../../platform2/common-mk/common_test.gypi'],
          'dependencies': ['libcamera-monitor'],
          'sources': [
            'camera-monitor/tools_unittest.cc',
          ],
        },
      ],
    },
    ],
  ],
}
