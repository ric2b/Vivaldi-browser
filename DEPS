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
        'vivaldi/chromium/third_party/WebKit/Tools/Scripts',  # See http://crbug.com/625877.
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
    # Update skia_commit_hash.h.
    'name': 'lastchange_skia',
    'pattern': '.',
    'action': ['python', 'vivaldi/chromium/build/util/lastchange.py',
               '-m', 'SKIA_COMMIT_HASH',
               '-s', 'vivaldi/chromium/third_party/skia',
               '--header', 'vivaldi/chromium/skia/ext/skia_commit_hash.h'],
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
      '--revision=37f2efe2518802e568d2b620309c0c4a939e52f1',
      '--overwrite',
      '--copy-dia-binaries',
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
    'name': 'devtools_install_node',
    'action': [ 'python',
                'vivaldi/chromium/third_party/WebKit/Source/devtools/scripts/local_node/node.py',
                '--running-as-hook',
                '--version',
    ],
  },

  # Pull down NPM dependencies for WebUI toolchain.
  {
    'name': 'webui_node_modules',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--extract',
                '--no_auth',
                '--bucket', 'chromium-nodejs',
                '-s', 'vivaldi/chromium/third_party/node/node_modules.tar.gz.sha1',
    ],
  },
  {
    'name': 'bootstrap-gn',
    'pattern': '.',
    'action': [
      'python',
      'vivaldi/scripts/rungn.py',
      '--refresh',
    ],
  },
]
