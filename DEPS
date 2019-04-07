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
    # Ensure that we don't accidentally reference any .pyc files whose
    # corresponding .py files have since been deleted.
    # We could actually try to avoid generating .pyc files, crbug.com/500078.
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
        'vivaldi/chromium/third_party/blink/renderer/build/scripts',
        'vivaldi/chromium/third_party/blink/tools',  # See http://crbug.com/625877.
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
    'condition': 'checkout_android',
    'action': [
      'python',
      'vivaldi/scripts/android_submodules.py',
    ],
  },
  {
    'name': 'sysroot_arm',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_arm',
    'action': ['python', 'vivaldi/chromium/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=arm'],
  },
  {
    'name': 'sysroot_arm64',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_arm64',
    'action': ['python', 'vivaldi/chromium/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=arm64'],
  },
  {
    'name': 'sysroot_x86',
    'pattern': '.',
    'condition': 'checkout_linux and (checkout_x86 or checkout_x64)',
    'action': ['python', 'vivaldi/chromium/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=x86'],
  },
  {
    'name': 'sysroot_x64',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_x64',
    'action': ['python', 'vivaldi/chromium/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=x64'],
  },
  {
    # Case-insensitivity for the Win SDK. Must run before win_toolchain below.
    'name': 'ciopfs_linux',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "linux"',
    'action': [ 'python',
                'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/ciopfs',
                '-s', 'vivaldi/chromium/build/ciopfs.sha1',
    ]
  },
  # Pull binutils for linux, enabled debug fission for faster linking /
  # debugging when used with clang on Ubuntu Precise.
  # https://code.google.com/p/chromium/issues/detail?id=352046
  {
    'name': 'binutils',
    'pattern': 'vivaldi/chromium/third_party/binutils',
    'condition': 'host_os == "linux"',
    'action': [
      'python',
      'vivaldi/chromium/third_party/binutils/download.py'
    ],
  },
  {
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang',
    'pattern': '.',
    'action': ['python',
      'vivaldi/chromium/tools/clang/scripts/update.py'],
  },
  {
    # Mac doesn't use lld so it's not included in the default clang bundle
    # there.  lld is however needed in win and Fuchsia cross builds, so
    # download it there. Should run after the clang hook.
    'name': 'lld/mac',
    'pattern': '.',
    'condition': 'host_os == "mac" and checkout_win',
    'action': ['python', 'vivaldi/chromium/tools/clang/scripts/download_lld_mac.py'],
  },
  {
    # Update LASTCHANGE.
    'name': 'lastchange',
    'pattern': '.',
    'action': ['python',
      'vivaldi/chromium/build/util/lastchange.py',
      '-o', 'vivaldi/chromium/build/util/LASTCHANGE'],
  },
  {
    # Update GPU lists version string (for gpu/config).
    'name': 'gpu_lists_version',
    'pattern': '.',
    'action': ['python', 'vivaldi/chromium/build/util/lastchange.py',
               '-m', 'GPU_LISTS_VERSION',
               '--revision-id-only',
               '--header', 'vivaldi/chromium/gpu/config/gpu_lists_version.h'],
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
    'action': ['python', 'vivaldi/chromium/build/util/lastchange.py',
      '-s', 'vivaldi/.',
      '--name-suffix', '_VIVALDI',
      '-o','vivaldi/chromium/build/util/LASTCHANGE.vivaldi'],
  },
  # Pull clang-format binaries using checked-in hashes.
  {
    'name': 'clang_format_win',
    'pattern': '.',
    'condition': 'host_os == "win"',
    'action': [ 'python',
      'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket', 'chromium-clang-format',
      '-s', 'vivaldi/chromium/buildtools/win/clang-format.exe.sha1'
    ],
  },
  {
    'name': 'clang_format_mac',
    'pattern': '.',
    'condition': 'host_os == "mac"',
    'action': [ 'python',
      'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket', 'chromium-clang-format',
      '-s', 'vivaldi/chromium/buildtools/mac/clang-format.sha1'
    ],
  },
  {
    'name': 'clang_format_linux',
    'pattern': '.',
    'condition': 'host_os == "linux"',
    'action': [ 'python',
      'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket', 'chromium-clang-format',
      '-s', 'vivaldi/chromium/buildtools/linux64/clang-format.sha1'
    ],
  },
  # Pull rc binaries using checked-in hashes.
  {
    'name': 'rc_win',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "win"',
    'action': [ 'python',
                'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'vivaldi/chromium/build/toolchain/win/rc/win/rc.exe.sha1',
    ],
  },
  {
    'name': 'rc_mac',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "mac"',
    'action': [ 'python',
                'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'vivaldi/chromium/build/toolchain/win/rc/mac/rc.sha1',
    ],
  },
  {
    'name': 'rc_linux',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "linux"',
    'action': [ 'python',
                'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'vivaldi/chromium/build/toolchain/win/rc/linux64/rc.sha1',
    ]
  },
 {
    'name': 'test_fonts',
    'pattern': '.',
    'action': [ 'python',
      'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--extract',
                '--no_auth',
                '--bucket', 'chromium-fonts',
                '-s', 'vivaldi/chromium/third_party/test_fonts/test_fonts.tar.gz.sha1',
    ],
  },
  # Pull order files for the win/clang build.
  {
    'name': 'orderfiles_win',
    'pattern': '.',
    'condition': 'checkout_win',
    'action': [ 'python',
                'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/orderfiles',
                '-d', 'vivaldi/chromium/chrome/build',
    ],
  },
  # Pull luci-go binaries (isolate, swarming) using checked-in hashes.
  # TODO(maruel): Remove, https://crbug.com/851596
  {
    'name': 'luci-go_win',
    'pattern': '.',
    'condition': 'host_os == "win"',
    'action': [ 'python',
      'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket', 'chromium-luci',
      '-d', 'vivaldi/chromium/tools/luci-go/win64'
    ],
  },
  {
    'name': 'luci-go_mac',
    'pattern': '.',
    'condition': 'host_os == "mac"',
    'action': [ 'python',
      'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket', 'chromium-luci',
      '-d', 'vivaldi/chromium/tools/luci-go/mac64'
    ],
  },
  {
    'name': 'luci-go_linux',
    'pattern': '.',
    'condition': 'host_os == "linux"',
    'action': [ 'python',
      'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket', 'chromium-luci',
      '-d', 'vivaldi/chromium/tools/luci-go/linux64'
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

  # Download Telemetry's binary dependencies via conditionals
    {
    'name': 'checkout_telemetry_binary_dependencies',
    'condition': 'checkout_telemetry_dependencies',
    'pattern': '.',
    'action': [ 'python',
                'vivaldi/chromium/third_party/catapult/telemetry/bin/fetch_telemetry_binary_dependencies',
    ],
  },
	#
  # Download Telemetry's benchmark binary dependencies via conditionals
  {
    'name': 'checkout_telemetry_benchmark_deps',
    'condition': 'checkout_telemetry_dependencies',
    'pattern': '.',
    'action': [ 'python',
                'vivaldi/chromium/tools/perf/fetch_benchmark_deps.py',
                '-f',
    ],
  },
  {
    'name': 'Fetch Android AFDO profile',
    'pattern': '.',
    'condition': 'checkout_android or checkout_linux',
    'action': ['python', 'vivaldi/chromium/chrome/android/profiles/update_afdo_profile.py'],
  },
  {
    # This downloads SDK extras and puts them in the
    # third_party/android_tools/sdk/extras directory.
    'name': 'sdkextras',
    'pattern': '.',
    'condition': 'checkout_android',
    # When adding a new sdk extras package to download, add the package
    # directory and zip file to .gitignore in third_party/android_tools.
    'action': ['python',
               'vivaldi/chromium/build/android/play_services/update.py',
               'download'
    ],
  },
  {
    'name': 'gvr_static_shim_android_arm',
    'pattern': '\\.sha1',
    'condition': 'checkout_android',
    'action': [ 'python',
                'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-gvr-static-shim',
                '-s', 'vivaldi/chromium/third_party/gvr-android-sdk/libgvr_shim_static_arm.a.sha1',
    ],
  },
  {
    'name': 'gvr_static_shim_android_arm64',
    'pattern': '\\.sha1',
    'condition': 'checkout_android',
    'action': [ 'python',
                'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-gvr-static-shim',
                '-s', 'vivaldi/chromium/third_party/gvr-android-sdk/libgvr_shim_static_arm64.a.sha1',
      ],
  },
   {
    'name': 'gvr_static_shim_custom_libcxx_android_arm',
    'pattern': '\\.sha1',
    'condition': 'checkout_android',
    'action': [ 'python',
                'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-gvr-static-shim',
                '-s', 'vivaldi/chromium/third_party/gvr-android-sdk/libgvr_shim_static_custom_libcxx_arm.a.sha1',
    ],
  },
  {
    'name': 'gvr_static_shim_custom_libcxx_android_arm64',
    'pattern': '\\.sha1',
    'condition': 'checkout_android',
    'action': [ 'python',
                'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-gvr-static-shim',
                '-s', 'vivaldi/chromium/third_party/gvr-android-sdk/libgvr_shim_static_custom_libcxx_arm64.a.sha1',
      ],
  },
 {
    'name': 'vr_controller_test_api',
    'pattern': '\\.sha1',
    'condition': 'checkout_android',
    'action': [ 'python',
                'vivaldi/chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-gvr-static-shim/controller_test_api',
                '-s', 'vivaldi/chromium/third_party/gvr-android-sdk/test-libraries/controller_test_api.aar.sha1',
    ],
  },
  # Download VR test APKs only if the environment variable is set
  {
    'name': 'vr_test_apks',
    'pattern': '.',
    'condition': 'checkout_android',
    'action': [ 'python',
               'vivaldi/chromium/third_party/gvr-android-sdk/test-apks/update.py',
    ],
  },
  # DOWNLOAD AR test APKs only if the environment variable is set
  {
    'name': 'ar_test_apks',
    'pattern': '.',
    'condition': 'checkout_android',
    'action': [ 'python',
                'vivaldi/chromium/third_party/arcore-android-sdk/test-apks/update.py',
    ],
  },
  {
    # Pull doclava binaries if building for Android.
    'name': 'doclava',
    'pattern': '.',
    'condition': 'checkout_android',
    'action': ['python',
               'vivaldi/chromium/build/android/download_doclava.py',
    ],
  },
  # Download and initialize "vpython" VirtualEnv environment packages.
  {
    'name': 'vpython_common',
    'pattern': '.',
    'condition': 'checkout_linux or checkout_mac',
    'action': [ 'vpython',
                '-vpython-spec', 'vivaldi/chromium/.vpython',
                '-vpython-tool', 'install',
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
  }
]
