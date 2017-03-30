# DO NOT EDIT EXCEPT FOR LOCAL TESTING.

hooks = [
    {
    # This clobbers when necessary (based on get_landmines.py). It must be the
    # first hook so that other things that get/generate into the output
    # directory will not subsequently be clobbered.
    'name': 'landmines',
    'pattern': '.',
    'action': [
      'python',
      'vivaldi/chromium/build/landmines.py'
      ],
    },
    {
    # Ensure that while generating dependencies lists in .gyp files we don't
    # accidentally reference any .pyc files whose corresponding .py files have
    # already been deleted.
    # We should actually try to avoid generating .pyc files, crbug.com/500078.
    'name': 'remove_stale_pyc_files',
    'pattern': '.',
    'action': [
      'python',
        'vivaldi/chromium/tools/remove_stale_pyc_files.py',
        'vivaldi/chromium/android_webview/tools',
        'vivaldi/chromium/build/android',
        'vivaldi/chromium/gpu/gles2_conform_support',
        'vivaldi/chromium/infra',
        'vivaldi/chromium/ppapi',
        'vivaldi/chromium/printing',
        'vivaldi/chromium/third_party/catapult',
        'vivaldi/chromium/third_party/closure_compiler/build',
        'vivaldi/chromium/tools',
    ],
  },
  {
    # Downloads the current stable linux sysroot to build/linux/ if needed.
    # This sysroot updates at about the same rate that the chrome build deps
    # change. This script is a no-op except for linux users who are doing
    # official chrome builds or cross compiling.
    'name': 'sysroot',
    'pattern': '.',
    'action': [
        'python',
      'vivaldi/chromium/build/linux/sysroot_scripts/install-sysroot.py',
      '--running-as-hook'
    ],
  },
  {
    # Update the Windows toolchain if necessary.
    'name': 'win_toolchain',
    'pattern': '.',
    'action': ['python', 'vivaldi/chromium/build/vs_toolchain.py', 'update'],
  },
  {
    # Update the Mac toolchain if necessary.
    'name': 'mac_toolchain',
    'pattern': '.',
    'action': ['python', 'vivaldi/chromium/build/mac_toolchain.py'],
  },
  # Pull binutils for linux, enabled debug fission for faster linking /
  # debugging when used with clang on Ubuntu Precise.
  # https://code.google.com/p/chromium/issues/detail?id=352046
  {
    'name': 'binutils',
    'pattern': 'vivaldi/chromium/third_party/binutils',
    'action': [
      'python',
      'vivaldi/chromium/third_party/binutils/download.py'
    ],
  },
  {
    # Pull clang if needed or requested via GYP_DEFINES.
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang',
    'pattern': '.',
    'action': [
      'python',
      'vivaldi/chromium/tools/clang/scripts/update.py',
      '--if-needed'
      ],
  },
  {
    # Update LASTCHANGE.
    'name': 'lastchange',
    'pattern': '.',
    'action': [
      'python',
      'vivaldi/chromium/build/util/lastchange.py',
      '--git-hash-only',
      '-o',
      'vivaldi/chromium/build/util/LASTCHANGE'
       ],
  },
  {
    # Update LASTCHANGE.blink.
    'name': 'lastchange_blink',
    'pattern': '.',
    'action': [
      'python',
      'vivaldi/chromium/build/util/lastchange.py',
      '--git-hash-only',
      '-s', 'vivaldi/chromium/third_party/WebKit',
      '-o', 'vivaldi/chromium/build/util/LASTCHANGE.blink'
      ],
  },
  {
    'name': 'lastchange_vivaldi',
    'pattern': '.',
    'action': [
      'python',
      'vivaldi/chromium/build/util/lastchange.py',
      '--git-hash-only',
      '-s', 'vivaldi/.',
      '--name-suffix', '_VIVALDI',
      '-o','vivaldi/chromium/build/util/LASTCHANGE.vivaldi'
      ],
  },
  # Pull GN binaries. This needs to be before running GYP below.
  {
    'name': 'gn_win',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
      '--no_resume',
      '--platform=win32',
      '--no_auth',
      '--bucket', 'chromium-gn',
      '-s', 'vivaldi/chromium/buildtools/win/gn.exe.sha1'
      ],
    },
    {
    'name': 'gn_mac',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
      '--no_resume',
      '--platform=darwin',
      '--no_auth',
      '--bucket', 'chromium-gn',
      '-s', 'vivaldi/chromium/buildtools/mac/gn.sha1'
      ],
    },
    {
    'name': 'gn_linux64',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
      '--no_resume',
      '--platform=linux*',
      '--no_auth',
      '--bucket', 'chromium-gn',
      '-s', 'vivaldi/chromium/buildtools/linux64/gn.sha1'
      ],
  },
  # Pull clang-format binaries using checked-in hashes.
  {
    'name': 'clang_format_win',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
      '--no_resume',
      '--platform=win32',
      '--no_auth',
      '--bucket', 'chromium-clang-format',
      '-s', 'vivaldi/chromium/buildtools/win/clang-format.exe.sha1'
    ],
  },
  {
    'name': 'clang_format_mac',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
      '--no_resume',
      '--platform=darwin',
      '--no_auth',
      '--bucket', 'chromium-clang-format',
      '-s', 'vivaldi/chromium/buildtools/mac/clang-format.sha1'
    ],
  },
  {
    'name': 'clang_format_linux',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
      '--no_resume',
      '--platform=linux*',
      '--no_auth',
      '--bucket', 'chromium-clang-format',
      '-s', 'vivaldi/chromium/buildtools/linux64/clang-format.sha1'
    ],
  },
  # Pull the prebuilt libc++ static library for mac.
  {
    'name': 'libcpp_mac',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=darwin',
                '--no_auth',
                '--bucket', 'chromium-libcpp',
                '-s', 'vivaldi/chromium/third_party/libc++-static/libc++.a.sha1',
    ],
  },
  # Pull luci-go binaries (isolate, swarming) using checked-in hashes.
  {
    'name': 'luci-go_win',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
      '--no_resume',
      '--platform=win32',
      '--no_auth',
      '--bucket', 'chromium-luci',
      '-d', 'vivaldi/chromium/tools/luci-go/win64'
    ],
  },
  {
    'name': 'luci-go_mac',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
      '--no_resume',
      '--platform=darwin',
      '--no_auth',
      '--bucket', 'chromium-luci',
      '-d', 'vivaldi/chromium/tools/luci-go/mac64'
    ],
  },
  {
    'name': 'luci-go_linux',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
      '--no_resume',
      '--platform=linux*',
      '--no_auth',
      '--bucket', 'chromium-luci',
      '-d', 'vivaldi/chromium/tools/luci-go/linux64'
    ],
  },
  # Pull eu-strip binaries using checked-in hashes.
  {
    'name': 'eu-strip',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
      '--no_resume',
      '--platform=linux*',
      '--no_auth',
      '--bucket', 'chromium-eu-strip',
      '-s', 'vivaldi/chromium/build/linux/bin/eu-strip.sha1'
      ],
  },
  {
    'name': 'drmemory',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
      '--no_resume',
      '--platform=win32',
      '--no_auth',
      '--bucket', 'chromium-drmemory',
      '-s', 'vivaldi/chromium/third_party/drmemory/drmemory-windows-sfx.exe.sha1'
      ],
  },
  # Pull the Syzygy binaries, used for optimization and instrumentation.
  {
    'name': 'syzygy-binaries',
    'pattern': '.',
    'action': ['python',
      'vivaldi/chromium/build/get_syzygy_binaries.py',
      '--output-dir', 'vivaldi/chromium/third_party/syzygy/binaries',
      '--revision=da2c31c5634ec66236ab5851a53c4497e2212bfc',
      '--overwrite'
    ],
  },
  # TODO(pmonette): Move include files out of binaries folder.
  {
    'name': 'kasko',
    'pattern': '.',
    'action': ['python',
      'vivaldi/chromium/build/get_syzygy_binaries.py',
      '--output-dir', 'vivaldi/chromium/third_party/kasko/binaries',
      '--revision=266a18d9209be5ca5c5dcd0620942b82a2d238f3',
      '--resource=kasko.zip',
      '--resource=kasko_symbols.zip',
      '--overwrite'
    ],
  },
  {
    'name': 'apache_win32',
    'pattern': '\\.sha1',
    'action': [ 'download_from_google_storage',
      '--no_resume',
      '--platform=win32',
      '--directory',
      '--recursive',
      '--no_auth',
      '--num_threads=16',
      '--bucket', 'chromium-apache-win32',
      'vivaldi/chromium/third_party/apache-win32'
    ],
  },
  {
    'name': 'blimp_fonts',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--extract',
                '--no_auth',
                '--bucket', 'chromium-fonts',
                '-s', 'vivaldi/chromium/third_party/blimp_fonts/font_bundle.tar.gz.sha1',
    ],
  },
  {
    # Pull sanitizer-instrumented third-party libraries if requested via
    # GYP_DEFINES.
    'name': 'instrumented_libraries',
    'pattern': '\\.sha1',
    'action': [
      'python',
      'vivaldi/chromium/third_party/instrumented_libraries/scripts/download_binaries.py'
    ],
  },
  {
    'name': 'gyp',
    'pattern': '.',
    'action': [
      'python',
      '-O',
      'vivaldi/chromium/build/gyp_chromium',
      '--depth',
      'vivaldi/chromium/.',
      '-I',
      'vivaldi/vivaldi_common.gypi',
      '-G',
      'output_dir=../out',
      'vivaldi/all.gyp'
    ],
  }
]
