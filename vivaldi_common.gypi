{
  'includes': [
    '<(DEPTH)/../vivaldi_sources.gypi',
    '<(DEPTH)/../vivaldi_product.gypi',
  ],
  'variables': {
    'variables':{
      'vivaldi_use_compilecache%': 0,
    },
    # Hack to make sure the variable is available later in the file.
    'vivaldi_use_compilecache%': '<(vivaldi_use_compilecache)',

    'VIVALDI': '<(DEPTH)/..',

    'branding': 'vivaldi',
    'build_branch%': 'master',
    # Using Chromium as branding in FFMpeg library
    'ffmpeg_branding': 'Chromium',

    'enable_prod_wallet_service': 0,
    'remoting': 0,
    'disable_nacl': '1',
    'disable_pnacl': 1,
    'grit_defines': ['-D', '_vivaldi',
                     '-E', 'CHROMIUM_BUILD=vivaldi',
                     '-E', 'VIVALDI_BUILD=vivaldi'],
    'proprietary_codecs': 1,

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
              'mac_sdk_min%': '10.8',
            }, {  # else OS!="ios"
              'mac_sdk_min%': '10.6',
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
    'include_dirs': [
      '<(VIVALDI)', # vivaldi root folder
    ],
    'defines': ['VIVALDI_BUILD','CHROMIUM_BUILD',
                'VIVALDI_RELEASE=<!(python <(VIVALDI)/scripts/get_preview.py "<(build_branch)")',
                ],
    'conditions': [
      ['buildtype=="Official"', {
        'defines': [
                    'VIVALDI_BETA_VERSION="<(official_product_kind_string)"'
                   ],
      }],
    ],
  },
}
