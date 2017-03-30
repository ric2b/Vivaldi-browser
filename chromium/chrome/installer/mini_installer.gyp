{
  'variables': {
    'chromium_code': 1,
    'version_py': '<(DEPTH)/build/util/version.py',
    'version_path': '../../chrome/VERSION',
    'msvs_use_common_release': 0,
    'msvs_use_common_linker_extras': 0,
    'mini_installer_internal_deps%': 0,
    'mini_installer_official_deps%': 0,
  },
  'includes': [
    '../../build/win_precompile.gypi',
    '../../../vivaldi_version.gypi',
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'mini_installer',
          'dependencies': [
            '<@(chrome_dll_project)',
            '../chrome.gyp:chrome',
            '../chrome.gyp:chrome_nacl_win64',
            '../chrome.gyp:default_extensions',
            '../chrome.gyp:setup',
            '../chrome.gyp:crash_service',
            'installer_tools.gyp:test_installer_sentinel',
            'mini_installer_version.gyp:mini_installer_version',
          ],
          'variables': {
            'sign_executables': [],
            'chrome_dll_project': [
              '../chrome.gyp:chrome_dll',
            ],
            'chrome_dll_path': [
              '<(PRODUCT_DIR)/vivaldi.dll',
            ],
            'output_dir': '<(PRODUCT_DIR)',
          },
          'includes': [
            'mini_installer.gypi',
          ],
        }
      ],
      'conditions': [
        ['test_isolation_mode != "noop"', {
          'targets': [
            {
              'target_name': 'mini_installer_tests_run',
              'type': 'none',
              'dependencies': [
                'mini_installer',
              ],
              'includes': [
                '../../build/isolate.gypi',
              ],
              'sources': [
                'mini_installer_tests.isolate',
              ],
            },
          ],
        }],
      ],
    }],
  ],
}
