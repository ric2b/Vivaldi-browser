{
  'variables': {
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
            'manifest_name':"mini_installer_signed",
          },

          'libraries': [
            '<(INTERMEDIATE_DIR)/../mini_installer_lib.lib',
          ],
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
          'includes': [ 'mini_installer_step.gypi', ],
        },
      ],
    }],
  ],
}
