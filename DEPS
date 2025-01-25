# DO NOT EDIT EXCEPT FOR LOCAL TESTING.

vars = {
  "upstream_commit_id": "I5bbf556ecd353886c9042669aa0afbfd7e7266e9",

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
    # Ensure we remove any file from disk that is no longer needed (e.g. after
    # hooks to native GCS deps migration).
    'name': 'remove_stale_files',
    'pattern': '.',
    'action': [
        'python3',
        'chromium/tools/remove_stale_files.py',
        'chromium/third_party/test_fonts/test_fonts.tar.gz', # Remove after 20240901
        'chromium/third_party/node/node_modules.tar.gz', # TODO: Remove after 20241201, see https://crbug.com/351092787
        'chromium/third_party/tfhub_models', # TODO: Remove after 20241211
    ],
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
      '--ios', Var("checkout_ios_str"),
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
    # Force checkout chromium again on main to recover from CIPD deleting two androidx files
    # when moving from v126 to 128
    'name': 'checkout_chromium_again',
    'pattern': '.',
    'condition': 'checkout_android',
    'action': [
      'git', "-C", "chromium", "checkout", "-f", "--",
      "third_party/androidx/",
    ],
  },
  {
    # Delete chromium/third_party/node/node_modules.tar.gz so that a new copy is downloaded
    # when moving back to v126 from 128
    'name': 'delete_node-modules_tar_gx',
    'pattern': '.',
    'action': [
      'rm', "-f", "chromium/third_party/node/node_modules.tar.gz",
    ],
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
    'condition': 'checkout_reclient and (checkout_win or checkout_mac)',
    'action': ['python3', "-u", 'chromium/tools/clang/scripts/update.py',
                  "--host-os", "linux",
                  "--output-dir",
                  "chromium/third_party/llvm-build/Release+Asserts_linux"],
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
  # Download PGO profiles.
  {
    'name': 'Fetch PGO profiles for win-arm64',
    'pattern': '.',
    'condition': 'checkout_pgo_profiles and checkout_win',
    'action': [ 'python3',
                'chromium/tools/update_pgo_profiles.py',
                '--target=win-arm64',
                'update',
                '--gs-url-base=chromium-optimization-profiles/pgo_profiles',
    ],
  },
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
                '--check-v8-revision',
                "--force",
                '--quiet',
    ],
  },
  #{ # Vivaldi don't do libc++ roll and reverts; disabling
  #  # Clean up build dirs for crbug.com/1337238.
  #  # After a libc++ roll and revert, .ninja_deps would get into a state
  #  # that breaks Ninja on Windows.
  #  # TODO(crbug.com/1409337): Remove this after updating Ninja 1.12 or newer.
  #  'name': 'del_ninja_deps_cache',
  #  'pattern': '.',
  #  'condition': 'host_os == "win"',
  #  'action': ['python3', 'chromium/build/del_ninja_deps_cache.py'],
  #},
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
