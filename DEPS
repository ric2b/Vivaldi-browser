# DO NOT EDIT EXCEPT FOR LOCAL TESTING.

vars = {
  "upstream_commit_id": "I37353dd0442200f61096e348dd2b8de846305002",

  # The path of the sysroots.json file.
  # This is used by vendor builds like Electron.
  'sysroots_json_path': 'build/linux/sysroot_scripts/sysroots.json',
}

hooks = [
  # Download and initialize "vpython" VirtualEnv environment packages for
  # Python3. We do this before running any other hooks so that any other
  # hooks that might use vpython don't trip over unexpected issues and
  # don't run slower than they might otherwise need to.
  {
    'name': 'vpython3_common',
    'pattern': '.',
    'action': [ 'vpython3',
                '-vpython-spec', 'chromium/.vpython3',
                '-vpython-tool', 'install',
    ],
  },
  {
    # This clobbers when necessary (based on get_landmines.py). This should
    # run as early as possible so that other things that get/generate into the
    # output directory will not subsequently be clobbered.
    'name': 'landmines',
    'pattern': '.',
    'action': [
      'python3', "-u",
      'chromium/build/landmines.py'
    ],
  },
  {
    # Ensure that the DEPS'd "depot_tools" has its self-update capability
    # disabled.
    'name': 'disable_depot_tools_selfupdate',
    'pattern': '.',
    'action': [
        'python3', "-u",
        'chromium/third_party/depot_tools/update_depot_tools_toggle.py',
        '--disable',
    ],
  },
  {
    'name': 'Ensure bootstrap',
    'pattern': '.',
    'action': ["bash", 'chromium/third_party/depot_tools/ensure_bootstrap'],
  },
  {
    # Ensure that we don't accidentally reference any .pyc files whose
    # corresponding .py files have since been deleted.
    # We could actually try to avoid generating .pyc files, crbug.com/500078.
    'name': 'remove_stale_pyc_files',
    'pattern': '.',
    'action': [
      'python3', "-u",
        'chromium/tools/remove_stale_pyc_files.py',
        'chromium/android_webview/tools',
        'chromium/build/android',
        'chromium/gpu/gles2_conform_support',
        'chromium/infra',
        'chromium/ppapi',
        'chromium/printing',
        'chromium/third_party/blink/renderer/build/scripts',
        'chromium/third_party/blink/tools',  # See http://crbug.com/625877.
        'chromium/third_party/catapult',
        'chromium/third_party/mako', # Some failures triggered by crrev.com/c/3686969
        'chromium/tools',
    ],
  },
  {
    'name': 'load_build_support',
    'pattern': '.',
    'action': [
      'python3', "-u", 'scripts/load_net_build_support.py',
      '--linux', Var("checkout_linux_str"),
      '--win', Var("checkout_win_str"),
      '--mac', Var("checkout_mac_str"),
      '--android', Var("checkout_android_str"),
      '--x64', Var("checkout_x64_str"),
      '--arm', Var("checkout_arm_str"),
      '--arm64', Var("checkout_arm64_str"),
    ],
  },
  {
    # Check out cipd submodules based on chromium/DEPS for all OSes
    'name': 'checkout_general_submodules',
    'pattern': '.',
    'action': [
      'python3', "-u", 'scripts/general_submodules.py',
    ],
  },
  {
    'name': 'sysroot_arm',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_arm',
    'action': ['python3', "-u", 'chromium/build/linux/sysroot_scripts/install-sysroot.py',
               '--sysroots-json-path=' + Var('sysroots_json_path'),
               '--arch=arm'],
  },
  {
    'name': 'sysroot_arm64',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_arm64',
    'action': ['python3', "-u", 'chromium/build/linux/sysroot_scripts/install-sysroot.py',
               '--sysroots-json-path=' + Var('sysroots_json_path'),
               '--arch=arm64'],
  },
  {
    'name': 'sysroot_x86',
    'pattern': '.',
    'condition': 'checkout_linux and (checkout_x86 or checkout_x64)',
    'action': ['python3', "-u", 'chromium/build/linux/sysroot_scripts/install-sysroot.py',
               '--sysroots-json-path=' + Var('sysroots_json_path'),
               '--arch=x86'],
  },
  {
    'name': 'sysroot_x64',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_x64',
    'action': ['python3', "-u", 'chromium/build/linux/sysroot_scripts/install-sysroot.py',
               '--sysroots-json-path=' + Var('sysroots_json_path'),
               '--arch=x64'],
  },
  {
    # Case-insensitivity for the Win SDK. Must run before win_toolchain below.
    'name': 'ciopfs_linux',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "linux"',
    'action': [ 'python3', "-u",
                'chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/ciopfs',
                '-s', 'chromium/build/ciopfs.sha1',
    ]
  },
  {
    # Update the Windows toolchain if necessary.  Must run before 'clang' below.
    'name': 'win_toolchain',
    'pattern': '.',
    'condition': 'checkout_win',
    'action': ['python3', "-u", 'scripts/vivaldi_update_toolchain.py'],
  },
  {
    # Update the prebuilt clang toolchain.
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang',
    'pattern': '.',
    'action': ['python3', "-u", 'chromium/tools/clang/scripts/update.py'],
  },
  {
    # Should run after the clang hook. Used on mac, as well as for orderfile
    # generation and size tooling on Android. Used by
    # dump-static-initializers.py on linux.
    'name': 'objdump',
    'pattern': '.',
    'condition': 'checkout_linux or checkout_mac  or checkout_android and host_os != "mac"',
    'action': ['python3', 'chromium/tools/clang/scripts/update.py',
               '--package=objdump'],
  },
  {
    # Update LASTCHANGE.
    'name': 'lastchange_vivaldi',
    'pattern': '.',
    'action': ['python3', "-u",
      'scripts/vivaldi_lastchange.py',
      "--source-dir", ".",
      "--revision-id-only",
      '-o', 'chromium/build/util/LASTCHANGE'],
  },
  {
    # Update LASTCHANGE.chromium
    'name': 'lastchange_chromium',
    'pattern': '.',
    'action': ['python3', "-u", 'chromium/build/util/lastchange.py',
                '-o', 'chromium/build/util/LASTCHANGE.chromium',
      "--filter", "^Change-Id: " + Var("upstream_commit_id")],
  },
  {
    # Update GPU lists version string (for gpu/config).
    'name': 'gpu_lists_version',
    'pattern': '.',
    'action': ['python3', "-u", 'chromium/build/util/lastchange.py',
               '-m', 'GPU_LISTS_VERSION',
               '--revision-id-only',
               '--header', 'chromium/gpu/config/gpu_lists_version.h'],
  },
  {
    # Update skia_commit_hash.h.
    'name': 'lastchange_skia',
    'pattern': '.',
    'action': ['python3', "-u", 'chromium/build/util/lastchange.py',
               '-m', 'SKIA_COMMIT_HASH',
               '-s', 'chromium/third_party/skia',
               '--header', 'chromium/skia/ext/skia_commit_hash.h'],
  },
  {
    # Update dawn_version.h.
    'name': 'lastchange_dawn',
    'pattern': '.',
    'action': ['python3', 'chromium/build/util/lastchange.py',
               '-s', 'chromium/third_party/dawn',
               '--revision', 'chromium/gpu/webgpu/DAWN_VERSION'],
  },
  # Pull dsymutil binaries using checked-in hashes.
  {
    'name': 'dsymutil_mac_arm64',
    'pattern': '.',
    'condition': 'host_os == "mac" and host_cpu == "arm64"',
    'action': [ 'python3',
                'chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang',
                '-s', 'chromium/tools/clang/dsymutil/bin/dsymutil.arm64.sha1',
                '-o', 'chromium/tools/clang/dsymutil/bin/dsymutil',
    ],
  },
  {
    'name': 'dsymutil_mac_x64',
    'pattern': '.',
    'condition': 'host_os == "mac" and host_cpu == "x64"',
    'action': [ 'python3',
                'chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang',
                '-s', 'chromium/tools/clang/dsymutil/bin/dsymutil.x64.sha1',
                '-o', 'chromium/tools/clang/dsymutil/bin/dsymutil',
    ],
  },
  # Pull clang-format binaries using checked-in hashes.
  {
    'name': 'clang_format_win',
    'pattern': '.',
    'condition': 'host_os == "win"',
    'action': [ 'python3', "-u",
                'chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'chromium/buildtools/win/clang-format.exe.sha1'
    ],
  },
  {
    'name': 'clang_format_mac_x64',
    'pattern': '.',
    'condition': 'host_os == "mac" and host_cpu == "x64"',
    'action': [ 'python3',
                'chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'chromium/buildtools/mac/clang-format.x64.sha1',
                '-o', 'chromium/buildtools/mac/clang-format',
    ],
  },
 {
    'name': 'clang_format_mac_arm64',
    'pattern': '.',
    'condition': 'host_os == "mac" and host_cpu == "arm64"',
    'action': [ 'python3',
                'chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'chromium/buildtools/mac/clang-format.arm64.sha1',
                '-o', 'chromium/buildtools/mac/clang-format',
    ],
  },
  {
    'name': 'clang_format_linux',
    'pattern': '.',
    'condition': 'host_os == "linux"',
    'action': [ 'python3', "-u",
                'chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'chromium/buildtools/linux64/clang-format.sha1'
    ],
  },
  # Pull rc binaries using checked-in hashes.
  {
    'name': 'rc_win',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "win"',
    'action': [ 'python3', "-u",
                'chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'chromium/build/toolchain/win/rc/win/rc.exe.sha1',
    ],
  },
  {
    'name': 'rc_mac',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "mac"',
    'action': [ 'python3', "-u",
                'chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'chromium/build/toolchain/win/rc/mac/rc.sha1',
    ],
  },
  {
    'name': 'rc_linux',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "linux"',
    'action': [ 'python3', "-u",
                'chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'chromium/build/toolchain/win/rc/linux64/rc.sha1',
    ]
  },
 {
    'name': 'test_fonts',
    'pattern': '.',
    'action': [ 'python3', "-u",
      'chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--extract',
                '--no_auth',
                '--bucket', 'chromium-fonts',
                '-s', 'chromium/third_party/test_fonts/test_fonts.tar.gz.sha1',
    ],
  },
  # Download test resources for opus, i.e. audio files.
  {
    'name': 'opus_test_files',
    'pattern': '.',
    'action': ['download_from_google_storage',
               '--no_auth',
               '--quiet',
               '--bucket', 'chromium-webrtc-resources',
               '-d', 'chromium/third_party/opus/tests/resources'],
  },
  # Pull down NPM dependencies for WebUI toolchain.
  {
    'name': 'webui_node_modules',
    'pattern': '.',
    'action': [ 'python3', "-u",
                'chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--extract',
                '--no_auth',
                '--bucket', 'chromium-nodejs',
                '-s', 'chromium/third_party/node/node_modules.tar.gz.sha1',
    ],
  },
  {
    'name': 'Fetch Android AFDO profile',
    'pattern': '.',
    'condition': 'checkout_android',
    'action': [ 'python3',
                'chromium/tools/download_optimization_profile.py',
                '--newest_state=chromium/chrome/android/profiles/newest.txt',
                '--local_state=chromium/chrome/android/profiles/local.txt',
                '--output_name=chromium/chrome/android/profiles/afdo.prof',
                '--gs_url_base=chromeos-prebuilt/afdo-job/llvm',
    ],
  },
  {
    'name': 'Fetch Android Arm AFDO profile',
    'pattern': '.',
    'condition': 'checkout_android',
    'action': [ 'python3',
                'chromium/tools/download_optimization_profile.py',
                '--newest_state=chromium/chrome/android/profiles/arm.newest.txt',
                '--local_state=chromium/chrome/android/profiles/arm.local.txt',
                '--output_name=chromium/chrome/android/profiles/arm.afdo.prof',
                '--gs_url_base=chromeos-prebuilt/afdo-job/llvm',
    ],
  },
  {
    'name': 'gvr_static_shim_android',
    'pattern': '\\.sha1',
    'condition': 'checkout_android',
    'action': [ 'python3', "-u",
                'chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-gvr-static-shim',
                '-d', 'chromium/third_party/gvr-android-sdk',
    ],
  },
  {
    'name': 'vr_controller_test_api',
    'pattern': '\\.sha1',
    'condition': 'checkout_android',
    'action': [ 'python3', "-u",
                'chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-gvr-static-shim/controller_test_api',
                '-s', 'chromium/third_party/gvr-android-sdk/test-libraries/controller_test_api.aar.sha1',
    ],
  },
  # Download VR test APKs only if the environment variable is set
  {
    'name': 'vr_test_apks',
    'pattern': '.',
    'condition': 'checkout_android',
    'action': [ 'python3', "-u",
               'chromium/third_party/gvr-android-sdk/test-apks/update.py',
    ],
  },
  # DOWNLOAD AR test APKs only if the environment variable is set
  {
    'name': 'ar_test_apks',
    'pattern': '.',
    'condition': 'checkout_android',
    'action': [ 'python3', "-u",
                'chromium/third_party/arcore-android-sdk/test-apks/update.py',
    ],
  },
  {
    # Pull doclava binaries if building for Android.
    'name': 'doclava',
    'pattern': '.',
    'condition': 'checkout_android',
    'action': ['python3', "-u",
               'chromium/build/android/download_doclava.py',
    ],
  },
  {
    'name': 'subresource-filter-ruleset',
    'pattern': '.',
    'action': [ 'python3', "-u",
                'chromium/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-ads-detection',
                '-s', 'chromium/third_party/subresource-filter-ruleset/data/UnindexedRules.sha1',
    ],
  },
  # Download PGO profiles.
  {
    'name': 'Fetch PGO profiles for win32',
    'pattern': '.',
    'condition': 'checkout_pgo_profiles and checkout_win',
    'action': [ 'python3',
                'chromium/tools/update_pgo_profiles.py',
                '--target=win32',
                'update',
                '--gs-url-base=chromium-optimization-profiles/pgo_profiles',
    ],
  },
  {
    'name': 'Fetch PGO profiles for win64',
    'pattern': '.',
    'condition': 'checkout_pgo_profiles and checkout_win',
    'action': [ 'python3',
                'chromium/tools/update_pgo_profiles.py',
                '--target=win64',
                'update',
                '--gs-url-base=chromium-optimization-profiles/pgo_profiles',
    ],
  },
  {
    'name': 'Fetch PGO profiles for mac',
    'pattern': '.',
    'condition': 'checkout_pgo_profiles and checkout_mac',
    'action': [ 'python3',
                'chromium/tools/update_pgo_profiles.py',
                '--target=mac',
                'update',
                '--gs-url-base=chromium-optimization-profiles/pgo_profiles',
    ],
  },
  {
    'name': 'Fetch PGO profiles for mac arm',
    'pattern': '.',
    'condition': 'checkout_pgo_profiles and (checkout_mac or checkout_android)',
    'action': [ 'python3',
                'chromium/tools/update_pgo_profiles.py',
                '--target=mac-arm',
                'update',
                '--gs-url-base=chromium-optimization-profiles/pgo_profiles',
    ],
  },
  {
    'name': 'Fetch PGO profiles for linux',
    'pattern': '.',
    'condition': 'checkout_pgo_profiles and checkout_linux',
    'action': [ 'python3',
                'chromium/tools/update_pgo_profiles.py',
                '--target=linux',
                'update',
                '--gs-url-base=chromium-optimization-profiles/pgo_profiles',
    ],
  },
  {
    'name': 'Fetch PGO profiles for android arm32',
    'pattern': '.',
    'condition': 'checkout_pgo_profiles and checkout_android',
    'action': [ 'python3',
                'chromium/tools/update_pgo_profiles.py',
                '--target=android-arm32',
                'update',
                '--gs-url-base=chromium-optimization-profiles/pgo_profiles',
    ],
  },
  {
    'name': 'Fetch PGO profiles for android arm64',
    'pattern': '.',
    'condition': 'checkout_pgo_profiles and checkout_android',
    'action': [ 'python3',
                'chromium/tools/update_pgo_profiles.py',
                '--target=android-arm64',
                'update',
                '--gs-url-base=chromium-optimization-profiles/pgo_profiles',
    ],
  },
  {
    'name': 'Fetch PGO profiles for V8 builtins',
    'pattern': '.',
    # Always download profiles on Android builds. The GN arg `is_official_build`
    # is required to consider the profiles during build time.
    'condition': 'checkout_pgo_profiles or checkout_android',
    'action': [ 'python3',
                'chromium/v8/tools/builtins-pgo/download_profiles.py',
                'download',
                '--depot-tools',
                'chromium/third_party/depot_tools',
    ],
  },
  {
    # Clean up build dirs for crbug.com/1337238.
    # After a libc++ roll and revert, .ninja_deps would get into a state
    # that breaks Ninja on Windows.
    # TODO(crbug.com/1409337): Remove this after updating Ninja 1.12 or newer.
    'name': 'del_ninja_deps_cache',
    'pattern': '.',
    'condition': 'host_os == "win"',
    'action': ['python3', 'chromium/build/del_ninja_deps_cache.py'],
  },
  {
    'name': 'bootstrap-gn',
    'pattern': '.',
    'action': [
      'python3', "-u",
      'scripts/rungn.py',
      '--refresh',
    ],
  }
]
