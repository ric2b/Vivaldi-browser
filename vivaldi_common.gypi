# Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved
{
  'includes': [
    '<(DEPTH)/../vivaldi_sources.gypi',
    '<(DEPTH)/../vivaldi_product.gypi',
  ],
  'variables': {
    'variables':{
      'vivaldi_use_compilecache%': 0,

       # Turns off the disabling of unittests performed by testing/disable_unittests.cpp
      'enable_disabled_unit_tests%': 0,
      'enable_permanently_disabled_unit_tests%': 0,
    },
    # Hack to make sure the variable is available later in the file.
    'vivaldi_use_compilecache%': '<(vivaldi_use_compilecache)',
    'enable_disabled_unit_tests%': '<(enable_disabled_unit_tests)',
    'enable_permanently_disabled_unit_tests%': '<(enable_permanently_disabled_unit_tests)',

    'VIVALDI': '<(DEPTH)/..',

    'branding': 'vivaldi',
    'build_branch%': 'master',
    # Using Chromium as branding in FFMpeg library
    'ffmpeg_branding': 'Chromium',
    'enable_widevine': 1,

    'enable_prod_wallet_service': 0,
    'remoting': 0,
    'disable_nacl': '1',
    'disable_pnacl': 1,
    'disable_newlib': 1,

    'test_isolation_mode': 'noop',

    'grit_defines': ['-D', '_vivaldi',
                     '-E', 'CHROMIUM_BUILD=vivaldi',
                     '-E', 'VIVALDI_BUILD=vivaldi'],
    'proprietary_codecs': 1,

    # Extra resource pak locales for Vivaldi from GRD files
    # When adding a new locale foo ALWAYS update
    #   chromium/chrome/tools/build/win/FILES.cfg: Add entry for 'locale/foo.pak'
    'locales': [
    ],

    # Locales about to be added, but not ready
    'vivaldi_pending_locales': [
        'be', 'eo', 'es-PE', 'eu', 'fy', 'gl', 'hy', 'io',
        'is', 'jbo', 'ka', 'ku', 'mk', 'sc', 'sq', 'nn',
    ],

    'conditions' : [
      ['os_posix==1 and OS!="mac" and OS!="ios"', {
        # On some versions of Linux, e.g. Ubuntu 14 official builds cannot use debian sysroot
        'disable_sysroot': 0,
        'linux_dump_symbols': 0,
      }],
      ['OS=="ios"', {
          'ios_product_name': 'Vivaldi',
          'conditions': [
            ['buildtype=="Official"', {
              'ios_breakpad': 1,
            }],
          ],
      }],
      ['OS=="mac" or OS=="ios"', {
        'mac_product_name': 'Vivaldi',
        'variables': {
          # Mac OS X SDK and deployment target support.  The SDK identifies
          # the version of the system headers that will be used, and
          # corresponds to the MAC_OS_X_VERSION_MAX_ALLOWED compile-time
          # macro.  "Maximum allowed" refers to the operating system version
          # whose APIs are available in the headers.  The deployment target
          # identifies the minimum system version that the built products are
          # expected to function on.  It corresponds to the
          # MAC_OS_X_VERSION_MIN_REQUIRED compile-time macro.  To ensure these
          # macros are available, #include <AvailabilityMacros.h>.  Additional
          # documentation on these macros is available at
          # http://developer.apple.com/mac/library/technotes/tn2002/tn2064.html#SECTION3
          # Chrome normally builds with the Mac OS X 10.6 SDK and sets the
          # deployment target to 10.6.  Other projects, such as O3D, may
          # override these defaults.

          # Normally, mac_sdk_min is used to find an SDK that Xcode knows
          # about that is at least the specified version. In official builds,
          # the SDK must match mac_sdk_min exactly. If the SDK is installed
          # someplace that Xcode doesn't know about, set mac_sdk_path to the
          # path to the SDK; when set to a non-empty string, SDK detection
          # based on mac_sdk_min will be bypassed entirely.
          'conditions': [
            ['OS=="ios"', {
              'mac_sdk_min%': '10.9',
            }, {  # else OS!="ios"
              'mac_sdk_min%': '10.9',
            }],
          ],
          'mac_sdk_path%': '',
        },

        'mac_sdk_min': '<(mac_sdk_min)',
        'mac_sdk_path': '<(mac_sdk_path)',

        'conditions': [
          ['buildtype=="Official"', {
            'mac_strip_release': 1,
            'mac_sdk': '<!(python <(DEPTH)/build/mac/find_sdk.py --verify <(mac_sdk_min) --sdk_path=<(mac_sdk_path))',
            # Enable uploading crash dumps.
            'mac_breakpad_uploads': 0,
            # Enable dumping symbols at build time for use by Mac Breakpad.
            'mac_breakpad': 1,
            # Enable Keystone auto-update support.
            'mac_keystone': 0,
          }],
        ],
      }],
      ['OS=="win"', {
        'conditions': [
          ['target_arch == "x64"', {
            'platform_suffix': '.x64',
          }, {
            'platform_suffix': '',
          }],
        ],
      }],
      ['OS=="win" or OS=="mac"', {
        'system_proprietary_codecs': 1,
      }, {
        'system_proprietary_codecs': 0,
      }],
      ['OS=="win" or OS=="mac" or chromeos==1', {
        'enable_rlz': 1,
      }],
      ['OS=="mac" and vivaldi_use_compilecache', {
        'chromium_mac_pch':0,
      }],
      ['OS=="win" and vivaldi_use_compilecache', {
        'fastbuild':1,
        'chromium_win_pch':0,
		    'win_z7':0,
      }],
      ['OS == "mac"', {
        'ffmpeg_component': '<(component)',
      }, {
        'ffmpeg_component': 'shared_library',
      }],
    ],
  },
  'conditions': [
    ['(OS=="linux" or OS=="mac") and vivaldi_use_compilecache', {
      'make_global_settings': [
        ['CC_wrapper', '<!(which ccache)'],
        ['CXX_wrapper', '<!(which ccache)'],
        ['CC.host_wrapper', '<!(which ccache)'],
        ['CXX.host_wrapper', '<!(which ccache)'],
      ],
    }],
    ['OS=="win" and vivaldi_use_compilecache', {
      'make_global_settings': [
        ['CC_wrapper', '<(clcache_exe)'],
        ['CXX_wrapper', '<(clcache_exe)'],
        ['CC.host_wrapper', '<(clcache_exe)'],
        ['CXX.host_wrapper', '<(clcache_exe)'],
      ],
    }],
  ],
  'target_defaults': {
    'includes': [
      '<(DEPTH)/../vivaldi_disabled_targets.gypi',
    ],
    'include_dirs': [
      '<(VIVALDI)', # vivaldi root folder
    ],
    'defines': ['VIVALDI_BUILD','CHROMIUM_BUILD',
                'VIVALDI_RELEASE=<!(python <(VIVALDI)/scripts/get_preview.py "<(build_branch)")',
                'WIDEVINE_CDM_VERSION_STRING="1.0.123.456"',
                'OMIT_CHROME_FRAME',
                ],
    'conditions': [
      ['buildtype=="Official"', {
        'defines': [
                    'VIVALDI_PRODUCT_VERSION="<(official_product_kind_string)"'
                   ],
        'conditions': [
          ['OS=="mac"', {
            'configurations': {
              'Release_Base': {
                'xcode_settings': {
                  'OTHER_CFLAGS': [
                    # The Google Chrome Framework dSYM generated by dsymutil has
                    # grown larger than 4GB, which dsymutil can't handle. Reduce
                    # the amount of debug symbols.
                    '-fno-standalone-debug',  # See http://crbug.com/479841
                  ]
                },
              },
            },
          }],
        ],
      }],
      ['OS=="win" and vivaldi_use_compilecache',{
        'defines': ['VIVALDI_WIN_COMPILE_CACHE'],
      }],
    ],
    'target_conditions' : [
      ['_target_build_file in vivaldi_disabled_gyp', {
        'disabled': 1,
      }],
      ['_target_qualified in vivaldi_disabled_qualified', {
        'disabled': 1,
      }],
      ['_target_name in vivaldi_disabled', {
        'disabled': 1,
      }],
      ['_target_name=="browser_tests" and _target_build_file=="<(DEPTH)/chrome/chrome.gyp"', {
        'sources': [
          '<@(vivaldi_browser_tests_sources)',
        ],
      }],
      ['_target_name in ["chrome_main_dll", "chrome_child_dll", "test_support_common"]', {
        'sources': [
          '<@(vivaldi_main_delegate)',
        ],
      }],
      ['_target_name == "chrome_initial" and OS != "win" and OS != "mac" and OS != "android"', {
        'sources': [
          '<@(vivaldi_main_delegate)',
        ],
      }],
      ['_target_name == "test_support_common"', {
        'sources': [
          '<@(vivaldi_unit_test_suite)',
        ],
      }],
      ['_target_name == "base" and _target_build_file=="<(DEPTH)/base/base.gyp"', {
        'sources': [
          '<@(vivaldi_base)',
        ],
      }],
      ['_target_name in ["base_static", "base_static_win64", "base_i18n_nacl"]', {
        'sources': [
          '<@(vivaldi_base_static)',
        ],
      }],
      ['_target_name == "browser"', {
        'sources': [
          '<@(vivaldi_browser_sources)',
        ],
      }],
      ['_target_name == "browser_ui"', {
        'sources': [
          '<@(vivaldi_browser_ui)',
        ],
        'conditions': [
          ['OS == "mac"', {
            'sources': [
              '<@(vivaldi_browser_ui_mac)',
            ],
          }],
          ['OS != "mac"', {
            'sources': [
              '<@(vivaldi_browser_ui_views)',
            ],
          }]
        ],
      }],
      ['_target_name == "setup"', {
        'sources': [
          '<@(vivaldi_setup)',
        ],
      }],
      ['_target_name == "gtest"', {
        'sources': [
          '<@(vivaldi_gtest_updater)',
        ],
        'conditions': [
          ['enable_disabled_unit_tests == 1', {
            'defines': [
              # Turns off the disabling of unittests performed by testing/disable_unittests.cpp
              'DONT_DISABLE_UNITTESTS',
            ],
          }],
          ['enable_permanently_disabled_unit_tests == 1', {
            'defines': [
              # Turns off the permanent disabling of unittests performed by testing/disable_unittests.cpp
              'DONT_PERMANENTLY_DISABLE_UNITTESTS',
            ],
          }],
        ],
      }],
      ['_target_name == "common_constants"', {
        'sources': [
          '<@(vivaldi_common_constants)',
        ],
      }],
      ['_target_name in ["chrome_dll", "chrome_main_dll"]', {
        'mac_bundle_resources': [
          '<@(vivaldi_nibs)',
        ],
      }],
      ['_target_name == "extensions_browser"', {
        'sources': [
          '<@(vivaldi_extensions_browser)',
        ],
      }],
      ['_target_name == "extensions_renderer"', {
        'sources': [
          '<@(vivaldi_extensions_renderer)',
        ],
      }],
      ['_target_name == "blink_web"', {
        'sources': [
          '<@(vivaldi_keycode_translation)',
        ],
      }],
      ['_target_name == "content"', {
        'sources': [
          '<@(vivaldi_keycode_translation)',
        ],
      }],
      ['OS=="mac" and _target_name == "chrome_initial"', {
        'mac_bundle_resources!': [
          '<@(vivaldi_remove_mac_resources_chrome_exe)',
        ],
        'mac_bundle_resources': [
          '<@(vivaldi_add_mac_resources_chrome_exe)',
        ],
      }],

    ],
  },
}
