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
    # Ensure that the DEPS'd "depot_tools" has its self-update capability
    # disabled.
    'name': 'disable_depot_tools_selfupdate',
    'pattern': '.',
    'action': [
        'python',
        'vivaldi/chromium/third_party/depot_tools/update_depot_tools_toggle.py',
        '--disable',
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
    # Check out Android specific submodules based on chromium/DEPS if android is enabled
    # Use build/enable_android_build.sh to enable Android build specific actions
    # Use build/disable_android_build.sh to disable Android build
    'name': 'checkout_android_submodules',
    'pattern': '.',
    'action': [
      'python',
      'vivaldi/scripts/android_submodules.py',
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
      '-o',
      'vivaldi/chromium/build/util/LASTCHANGE'
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
      '-s', 'vivaldi/.',
      '--name-suffix', '_VIVALDI',
      '-o','vivaldi/chromium/build/util/LASTCHANGE.vivaldi'
      ],
  },
  # Pull clang-format binaries using checked-in hashes.
  {
    'name': 'clang_format_win',
    'pattern': '.',
    'action': [ 'python',
      'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
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
    'action': [ 'python',
      'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
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
    'action': [ 'python',
      'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
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
    'action': [ 'python',
      'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
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
    'action': [ 'python',
      'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
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
    'action': [ 'python',
      'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--platform=linux*',
      '--no_auth',
      '--bucket', 'chromium-luci',
      '-d', 'vivaldi/chromium/tools/luci-go/linux64'
    ],
  },
  {
    'name': 'drmemory',
    'pattern': '.',
    'action': [ 'python',
      'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
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
      '--revision=190dbfe74c6f5b5913820fa66d9176877924d7c5',
      '--overwrite',
      '--copy-dia-binaries',
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
    'action': [ 'python',
                'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--extract',
                '--no_auth',
                '--bucket', 'chromium-nodejs',
                '-s', 'vivaldi/chromium/third_party/node/node_modules.tar.gz.sha1',
    ],
  },

  # Download Telemetry's binary dependencies
  {
    'name': 'fetch_telemetry_binary_dependencies',
    'pattern': '.',
    'action': [ 'python',
                'vivaldi/chromium/tools/perf/conditionally_execute',
                '--gyp-condition', 'fetch_telemetry_dependencies=1',
                'vivaldi/chromium/third_party/catapult/telemetry/bin/fetch_telemetry_binary_dependencies',
    ],
  },
]


# Note: These are keyed off target os, not host os. So don't move things here
# that depend on the target os.
hooks_os = {
  'android': [
    {
      # This downloads SDK extras and puts them in the
      # third_party/android_tools/sdk/extras directory.
      'name': 'sdkextras',
      'pattern': '.',
      # When adding a new sdk extras package to download, add the package
      # directory and zip file to .gitignore in third_party/android_tools.
      'action': ['python',
                 'vivaldi/chromium/build/android/play_services/update.py',
                 'download'
      ],
    },
    {
      'name': 'android_system_sdk',
      'pattern': '.',
      'action': ['python',
                 'vivaldi/chromium/build/android/update_deps/update_third_party_deps.py',
                 'download',
                 '-b', 'android_system_stubs',
                 '-l', 'third_party/android_system_sdk'
      ],
    },
    {
      'name': 'intellij',
      'pattern': '.',
      'action': ['python',
                 'vivaldi/chromium/build/android/update_deps/update_third_party_deps.py',
                 'download',
                 '-b', 'chromium-intellij',
                 '-l', 'third_party/intellij'
      ],
    },
    {
      'name': 'javax_inject',
      'pattern': '.',
      'action': ['python',
                 'vivaldi/chromium/build/android/update_deps/update_third_party_deps.py',
                 'download',
                 '-b', 'chromium-javax-inject',
                 '-l', 'third_party/javax_inject'
      ],
    },
    {
      'name': 'hamcrest',
      'pattern': '.',
      'action': ['python',
                 'vivaldi/chromium/build/android/update_deps/update_third_party_deps.py',
                 'download',
                 '-b', 'chromium-hamcrest',
                 '-l', 'third_party/hamcrest'
      ],
    },
    {
      'name': 'guava',
      'pattern': '.',
      'action': ['python',
                 'vivaldi/chromium/build/android/update_deps/update_third_party_deps.py',
                 'download',
                 '-b', 'chromium-guava',
                 '-l', 'third_party/guava'
      ],
    },
    {
      'name': 'android_support_test_runner',
      'pattern': '.',
      'action': ['python',
                 'vivaldi/chromium/build/android/update_deps/update_third_party_deps.py',
                 'download',
                 '-b', 'chromium-android-support-test-runner',
                 '-l', 'third_party/android_support_test_runner'
      ],
    },
    {
      'name': 'byte_buddy',
      'pattern': '.',
      'action': ['python',
                 'vivaldi/chromium/build/android/update_deps/update_third_party_deps.py',
                 'download',
                 '-b', 'chromium-byte-buddy',
                 '-l', 'third_party/byte_buddy'
      ],
    },
    {
      'name': 'espresso',
      'pattern': '.',
      'action': ['python',
                 'vivaldi/chromium/build/android/update_deps/update_third_party_deps.py',
                 'download',
                 '-b', 'chromium-espresso',
                 '-l', 'third_party/espresso'
      ],
    },
    {
      'name': 'robolectric_libs',
      'pattern': '.',
      'action': ['python',
                 'vivaldi/chromium/build/android/update_deps/update_third_party_deps.py',
                 'download',
                 '-b', 'chromium-robolectric',
                 '-l', 'third_party/robolectric'
      ],
    },
    {
      'name': 'apache_velocity',
      'pattern': '.',
      'action': ['python',
                 'vivaldi/chromium/build/android/update_deps/update_third_party_deps.py',
                 'download',
                 '-b', 'chromium-apache-velocity',
                 '-l', 'third_party/apache_velocity'
      ],
    },
    {
      'name': 'ow2_asm',
      'pattern': '.',
      'action': ['python',
                 'vivaldi/chromium/build/android/update_deps/update_third_party_deps.py',
                 'download',
                 '-b', 'chromium-ow2-asm',
                 '-l', 'third_party/ow2_asm'
      ],
    },
    {
      'name': 'desugar',
      'pattern': '.',
      'action': ['python',
                 'vivaldi/chromium/build/android/update_deps/update_third_party_deps.py',
                 'download',
                 '-b', 'chromium-android-tools/bazel/desugar',
                 '-l', 'third_party/bazel/desugar'
      ],
    },
    {
      'name': 'apk-patch-size-estimator',
      'pattern': '.',
      'action': ['python',
                 'vivaldi/chromium/build/android/update_deps/update_third_party_deps.py',
                 'download',
                 '-b', 'chromium-android-tools/apk-patch-size-estimator',
                 '-l', 'third_party/apk-patch-size-estimator/lib'
      ],
    },
    {
      'name': 'icu4j',
      'pattern': '.',
      'action': ['python',
                 'vivaldi/chromium/build/android/update_deps/update_third_party_deps.py',
                 'download',
                 '-b', 'chromium-icu4j',
                 '-l', 'third_party/icu4j'
      ],
    },
    {
      'name': 'accessibility_test_framework',
      'pattern': '.',
      'action': ['python',
                 'vivaldi/chromium/build/android/update_deps/update_third_party_deps.py',
                 'download',
                 '-b', 'chromium-accessibility-test-framework',
                 '-l', 'third_party/accessibility_test_framework'
      ],
    },
    {
      'name': 'bouncycastle',
      'pattern': '.',
      'action': ['python',
                 'vivaldi/chromium/build/android/update_deps/update_third_party_deps.py',
                 'download',
                 '-b', 'chromium-bouncycastle',
                 '-l', 'third_party/bouncycastle'
      ],
    },
    {
      'name': 'sqlite4java',
      'pattern': '.',
      'action': ['python',
                 'vivaldi/chromium/build/android/update_deps/update_third_party_deps.py',
                 'download',
                 '-b', 'chromium-sqlite4java',
                 '-l', 'third_party/sqlite4java'
      ],
    },
    {
      'name': 'gson',
      'pattern': '.',
      'action': ['python',
                 'vivaldi/chromium/build/android/update_deps/update_third_party_deps.py',
                 'download',
                 '-b', 'chromium-gson',
                 '-l', 'third_party/gson',
      ],
    },
    {
      'name': 'objenesis',
      'pattern': '.',
      'action': ['python',
                 'vivaldi/chromium/build/android/update_deps/update_third_party_deps.py',
                 'download',
                 '-b', 'chromium-objenesis',
                 '-l', 'third_party/objenesis'
      ],
    },
    {
      'name': 'xstream',
      'pattern': '.',
      'action': ['python',
                 'vivaldi/chromium/build/android/update_deps/update_third_party_deps.py',
                 'download',
                 '-b', 'chromium-robolectric',
                 '-l', 'third_party/xstream'
      ],
    },
    {
      'name': 'gvr_static_shim_android_arm',
      'pattern': '\\.sha1',
    'action': [   'python',
                  'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
                  '--no_resume',
                  '--platform=linux*',
                  '--no_auth',
                  '--bucket', 'chromium-gvr-static-shim',
                  '-s', 'vivaldi/chromium/third_party/gvr-android-sdk/libgvr_shim_static_arm.a.sha1',
      ],
    },
    {
      'name': 'gvr_static_shim_android_arm64',
      'pattern': '\\.sha1',
    'action': [   'python',
                  'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
                  '--no_resume',
                  '--platform=linux*',
                  '--no_auth',
                  '--bucket', 'chromium-gvr-static-shim',
                  '-s', 'vivaldi/chromium/third_party/gvr-android-sdk/libgvr_shim_static_arm64.a.sha1',
      ],
    },
    {
      'name': 'vr_controller_test_api',
      'pattern': '\\.sha1',
      'action': [   'python',
                  'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
                  '--no_resume',
                  '--platform=linux*',
                  '--no_auth',
                  '--bucket', 'chromium-gvr-static-shim/controller_test_api',
                  '-s', 'vivaldi/chromium/third_party/gvr-android-sdk/test-libraries/controller_test_api.aar.sha1',
      ],
    },
    # Download VR test APKs only if the environment variable is set
    {
      'name': 'vr_test_apks',
      'pattern': '.',
      'action': [ 'python',
                 'vivaldi/chromium/third_party/gvr-android-sdk/test-apks/update.py',
      ],
    },
    {
      # Pull doclava binaries if building for Android.
      'name': 'doclava',
      'pattern': '.',
      'action': ['python',
                 'vivaldi/chromium/build/android/download_doclava.py',
      ],
    },
  ],
}
