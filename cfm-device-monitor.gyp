# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libudev',
        'libusb-1.0',
      ],
    },
    'include_dirs': [
      # We assume the cwd is named "cfm-device-monitor" for #include paths.
      '..',
    ],
  },
  'targets': [
    {
      'target_name': 'libcamera-monitor',
      'type': 'static_library',
      'sources': [
        'camera-monitor/abstract_monitor.cc',
        'camera-monitor/huddly_monitor.cc',
        'camera-monitor/tools.cc',
        'camera-monitor/udev.cc',
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
    {
      'target_name': 'libmimo-monitor',
      'type': 'static_library',
      'sources': [
        'mimo-monitor/displaylink_monitor.cc',
        'mimo-monitor/mimo_monitor.cc',
        'mimo-monitor/sis_monitor.cc',
        'mimo-monitor/utils.cc',
      ],
    },
    {
      'target_name': 'mimo-monitor',
      'type': 'executable',
      'dependencies': [
        'libmimo-monitor',
      ],
      'sources': [
        'mimo-monitor/main.cc',
      ],
    },
    {
      'target_name': 'apex-monitor',
      'type': 'executable',
      'sources': [
        'apex-monitor/apex_manager.cc',
        'apex-monitor/apex_monitor.cc',
        'apex-monitor/i2c_interface.cc',
        'apex-monitor/main.cc',
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
        {
          'target_name': 'camera-hotplug-test',
          'type': 'executable',
          'dependencies': ['libcamera-monitor'],
          'sources': [
            'camera-monitor/hotplug_test.cc',
          ],
        },
	{
          'target_name': 'apex-manager-test',
          'type': 'executable',
          'includes': ['../../platform2/common-mk/common_test.gypi'],
          'sources': [
            'apex-monitor/apex_manager.cc',
            'apex-monitor/apex_manager_test.cc',
            'apex-monitor/fake_i2c_interface.cc',
          ],
	},
	{
          'target_name': 'apex-monitor-test',
          'type': 'executable',
          'includes': ['../../platform2/common-mk/common_test.gypi'],
          'variables': {
            'deps': [
              'libbrillo-test-<(libbase_ver)',
              'libchrome-test-<(libbase_ver)',
              'libcros_config',
            ],
          },
          'sources': [
            'apex-monitor/apex_manager.cc',
            'apex-monitor/apex_monitor.cc',
            'apex-monitor/apex_monitor_test.cc',
            'apex-monitor/fake_i2c_interface.cc',
          ],
	},
      ],
    },],
  ],
}
