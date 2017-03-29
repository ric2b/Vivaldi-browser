{
  'variables': {
    'version_py': '<(DEPTH)/build/util/version.py',
    'version_path': '../../chrome/VERSION',
    'lastchange_path': '<(DEPTH)/build/util/LASTCHANGE',
    'branding_dir': '../app/theme/<(branding_path_component)',
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
      'includes': [
        '../test/mini_installer/test_installer.gypi',
      ],
      'targets': [
        {
	  # A target that is outdated if any of the mini_installer test sources
	  # are modified.
          'target_name': 'test_installer_sentinel',
          'type': 'none',
          'actions': [
            {
              'action_name': 'touch_sentinel',
	      'variables': {
	        'touch_sentinel_py': '../tools/build/win/touch_sentinel.py',
              },
              'inputs': [
                '<@(test_installer_sources)',  # from test_installer.gypi
	        '<(touch_sentinel_py)',
              ],
              'outputs': ['<(SHARED_INTERMEDIATE_DIR)/chrome/installer/test_installer_sentinel'],
              'action': ['python', '<(touch_sentinel_py)', '<@(_outputs)'],
            },
          ],
        },
        {
          'target_name': 'mini_installer_lib',
          'type': 'static_library',

          'dependencies': [
            '../chrome.gyp:vivaldi',
            '../chrome.gyp:chrome_nacl_win64',
            '../chrome.gyp:chrome_dll',
            '../chrome.gyp:default_extensions',
            '../chrome.gyp:setup',
            'test_installer_sentinel',
          ],
          'include_dirs': [
            '../..',
            '<(INTERMEDIATE_DIR)',
            '<(SHARED_INTERMEDIATE_DIR)/chrome',
          ],
          'sources': [
            'mini_installer/appid.h',
            'mini_installer/chrome_appid.cc',
            'mini_installer/configuration.cc',
            'mini_installer/configuration.h',
            'mini_installer/decompress.cc',
            'mini_installer/decompress.h',
            'mini_installer/exit_code.h',
            'mini_installer/mini_installer.cc',
            'mini_installer/mini_installer.ico',
            'mini_installer/mini_installer_constants.cc',
            'mini_installer/mini_installer_constants.h',
            'mini_installer/mini_installer_exe_version.rc.version',
            'mini_installer/mini_string.cc',
            'mini_installer/mini_string.h',
            'mini_installer/pe_resource.cc',
            'mini_installer/pe_resource.h',
          ],

          # Disable precompiled headers for this project, to avoid
          # linker errors when building with VS 2008.
          'msvs_precompiled_header': '',
          'msvs_precompiled_source': '',

          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],

          'rules': [
            {
              'rule_name': 'mini_installer_version',
              'extension': 'version',
              'variables': {
                'template_input_path': 'mini_installer/mini_installer_exe_version.rc.version',
              },
              'inputs': [
                '<(template_input_path)',
                '<(version_path)',
                '<(vivaldi_version_path)',
                '<(lastchange_path)',
                '<(branding_dir)/BRANDING',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/mini_installer_exe_version.rc',
              ],
              'action': [
                'python', '<(version_py)',
                '-f', '<(version_path)',
                '-f', '<(lastchange_path)',
                '-f', '<(branding_dir)/BRANDING',
                '-f', '<(vivaldi_version_path)',
                '-e', 'VIVALDI_BUILD=<(vivaldi_global_build_number)',
                '<(template_input_path)',
                '<@(_outputs)',
              ],
              'message': 'Generating version information',
            },
          ],
        },
        { 
          'target_name': 'mini_installer',
          'type': 'executable',
          'product_name': 'Vivaldi.<(version_full)<(platform_suffix)',

          'variables':{
            'sign_executables': [],
            'manifest_name':'mini_installer',
          },
          'dependencies': [
            '../chrome.gyp:vivaldi',
            '../chrome.gyp:chrome_nacl_win64',
            '../chrome.gyp:chrome_dll',
            '../chrome.gyp:default_extensions',
            '../chrome.gyp:setup',
            'mini_installer_lib',
          ],
          # Do all the building here
          'includes': [ 'mini_installer_step.gypi', ],
        },
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
