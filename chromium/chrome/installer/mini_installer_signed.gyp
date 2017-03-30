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
    '../../../get_vivaldi_version.gypi'
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'vivaldi_installer_signed',
          'type': 'executable',

          'variables':{
            'sign_executables': ['--sign-executables'],
            'chrome_dll_project': [
              '../chrome.gyp:chrome_dll',
            ],
            'chrome_dll_path': [
              '<(PRODUCT_DIR)/vivaldi.dll',
            ],
            'output_dir': '<(PRODUCT_DIR)',
          },
          'conditions' : [
            ['icu_use_data_file_flag == 0', {
              'sources': [
                '<(PRODUCT_DIR)/icudt.dll',
              ],
            }, { # else icu_use_data_file_flag != 0
              'sources': [
                '<(PRODUCT_DIR)/icudtl.dat',
              ],
            }],
          ],
          # Do all the building here
          'includes': [ 'mini_installer.gypi', ],
        },
      ],
    }],
  ],
}
