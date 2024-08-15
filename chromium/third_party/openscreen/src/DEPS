# This file is used to manage the dependencies of the Open Screen repo. It is
# used by gclient to determine what version of each dependency to check out.
#
# For more information, please refer to the official documentation:
#   https://sites.google.com/a/chromium.org/dev/developers/how-tos/get-the-code
#
# When adding a new dependency, please update the top-level .gitignore file
# to list the dependency's destination directory.

use_relative_paths = True
git_dependencies = 'SYNC'

gclient_gn_args_file = 'build/config/gclient_args.gni'
gclient_gn_args = [
  'build_with_chromium',
]

vars = {
  'boringssl_git': 'https://boringssl.googlesource.com',
  'chromium_git': 'https://chromium.googlesource.com',
  'quiche_git': 'https://quiche.googlesource.com',

  # NOTE: we should only reference GitHub directly for dependencies toggled
  # with the "not build_with_chromium" condition.
  'github': 'https://github.com',

  # NOTE: Strangely enough, this will be overridden by any _parent_ DEPS, so
  # in Chromium it will correctly be True.
  'build_with_chromium': False,

  # Needed to download additional clang binaries for processing coverage data
  # (from binaries with GN arg `use_coverage=true`).
  #
  # TODO(issuetracker.google.com/155195126): Change this to False and update
  # buildbot to call tools/download-clang-update-script.py instead.
  'checkout_clang_coverage_tools': True,

 # Fetch clang-tidy into the same bin/ directory as our clang binary.
  'checkout_clang_tidy': False,

  # Fetch clangd into the same bin/ directory as our clang binary.
  'checkout_clangd': False,

  # Fetch instrumented libraries for using MSAN builds.
  'checkout_configuration': 'default',
  'checkout_instrumented_libraries': 'checkout_linux and checkout_configuration == "default"',

  # GN CIPD package version.
  'gn_version': 'git_revision:5e19d2fb166fbd4f6f32147fbb2f497091a54ad8',
  'clang_format_revision': 'e435ad79c17b1888b34df88d6a30a094936e3836',

  # 'magic' text to tell depot_tools that git submodules should be accepted
  # but parity with DEPS file is expected.
  'SUBMODULE_MIGRATION': 'True',

  # The path of the sysroots.json file.
  'sysroots_json_path': 'build/linux/sysroot_scripts/sysroots.json',

  # This can be overridden, e.g. with custom_vars, to build clang from HEAD
  # instead of downloading the prebuilt pinned revision.
  'llvm_force_head_revision': False,
}

deps = {
  # NOTE: These commit hashes here reference a repository/branch that is a
  # mirror of the commits in the corresponding Chromium repository directory,
  # and should be regularly updated with the tip of the MIRRORED master branch,
  # found here:
  # https://chromium.googlesource.com/chromium/src/buildtools/+/refs/heads/main
  'buildtools': {
    'url': Var('chromium_git') + '/chromium/src/buildtools' +
      '@' + '0ac67b7cef80e421283f633ee9c2ce652f6e42cc',
  },

  # and here:
  # https://chromium.googlesource.com/chromium/src/buildtools/+/refs/heads/main
  'build': {
    'url': Var('chromium_git') + '/chromium/src/build' +
      '@' + '63724e6b5f8e6b25b0b77302a393102996c1ddf3',
    'condition': 'not build_with_chromium',
  },

  'third_party/clang-format/script': {
    'url': Var('chromium_git') +
      '/external/github.com/llvm/llvm-project/clang/tools/clang-format.git' +
      '@' + Var('clang_format_revision'),
    'condition': 'not build_with_chromium',
  },
  'buildtools/linux64': {
    'packages': [
      {
        'package': 'gn/gn/linux-amd64',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "linux" and not build_with_chromium',
  },
  'buildtools/mac': {
    'packages': [
      {
        'package': 'gn/gn/mac-${{arch}}',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "mac" and not build_with_chromium',
  },
  'buildtools/win': {
    'packages': [
      {
        'package': 'gn/gn/windows-amd64',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "win"',
  },

  'third_party/ninja': {
    'packages': [
      # https://chrome-infra-packages.appspot.com/p/infra/3pp/tools/ninja
      {
        'package': 'infra/3pp/tools/ninja/${{platform}}',
        'version': 'version:2@1.8.2.chromium.3',
      }
    ],
    'dep_type': 'cipd',
    'condition': 'not build_with_chromium',
  },

  'third_party/libprotobuf-mutator/src': {
    'url': Var('chromium_git') +
      '/external/github.com/google/libprotobuf-mutator.git' +
      '@' + 'a304ec48dcf15d942607032151f7e9ee504b5dcf',
    'condition': 'not build_with_chromium',
  },

  'third_party/zlib/src': {
    'url': Var('github') +
      '/madler/zlib.git' +
      '@' + '04f42ceca40f73e2978b50e93806c2a18c1281fc', # version 1.2.13
    'condition': 'not build_with_chromium',
  },

  'third_party/jsoncpp/src': {
    'url': Var('chromium_git') +
      '/external/github.com/open-source-parsers/jsoncpp.git' +
      '@' + '5defb4ed1a4293b8e2bf641e16b156fb9de498cc', # version 1.9.5
    'condition': 'not build_with_chromium',
  },

  # googletest now recommends "living at head," which is a bit of a crapshoot
  # because regressions land upstream frequently.  This is a known good revision.
  'third_party/googletest/src': {
    'url': Var('chromium_git') +
      '/external/github.com/google/googletest.git' +
      '@' + 'b495f72f1f096135cf9cf8c7879b5b89250de50a',  # 2023-01-25
    'condition': 'not build_with_chromium',
  },

  # Note about updating BoringSSL: after changing this hash, run the update
  # script in BoringSSL's util folder for generating build files from the
  # <openscreen src-dir>/third_party/boringssl directory:
  # python3 ./src/util/generate_build_files.py --embed_test_data=false gn
  'third_party/boringssl/src': {
    'url' : Var('boringssl_git') + '/boringssl.git' +
      '@' + '8d19c850d4dbde4bd7ece463c3b3f3685571a779',
    'condition': 'not build_with_chromium',
  },

  # To roll forward, use quiche_revision from chromium/src/DEPS.
  'third_party/quiche/src': {
    'url': Var('quiche_git') + '/quiche.git' +
      '@' + '45374cbe5557a6d3da7aa1a43c969314f7b1894e',  # 2023-11-23
    'condition': 'not build_with_chromium',
  },

  'third_party/instrumented_libraries': {
    'url': Var('chromium_git') + '/chromium/src/third_party/instrumented_libraries.git' +
      '@' + '0011c28c8d35fc5093bb29631d05428932cd1206',
    'condition': 'not build_with_chromium',
  },

  'third_party/tinycbor/src':
    Var('chromium_git') + '/external/github.com/intel/tinycbor.git' +
    '@' +  'd393c16f3eb30d0c47e6f9d92db62272f0ec4dc7',  # Version 0.6.0

  # Abseil recommends living at head; we take a revision from one of the LTS
  # tags.  Chromium has forked abseil for reasons and it seems to be rolled
  # frequently, but LTS should generally be safe for interop with Chromium code.
  'third_party/abseil/src': {
    'url': Var('chromium_git') +
      '/external/github.com/abseil/abseil-cpp.git' + '@' +
      'a39d71a80fbdf0ee2f00222943c1ca08361cec20',  # lts_2023_01_25
    'condition': 'not build_with_chromium',
  },

  'third_party/libfuzzer/src': {
    'url': Var('chromium_git') +
      '/chromium/llvm-project/compiler-rt/lib/fuzzer.git' +
      '@' + 'debe7d2d1982e540fbd6bd78604bf001753f9e74',
    'condition': 'not build_with_chromium',
  },

  'third_party/libc++/src': {
    'url': Var('chromium_git') +
    '/external/github.com/llvm/llvm-project/libcxx.git' + '@' + 'caccdb0407e84357ca6490165e88dcad64e47d17',
    'condition': 'not build_with_chromium',
  },

  'third_party/libc++abi/src': {
    'url': Var('chromium_git') +
    '/external/github.com/llvm/llvm-project/libcxxabi.git' + '@' + '4cb5c2cefedc025433f81735bacbc0f773fdcd8f',
    'condition': 'not build_with_chromium',
  },

  'third_party/modp_b64': {
    'url': Var('chromium_git') + '/chromium/src/third_party/modp_b64'
    '@' + '3643752c065d984647f0ded68a9a01926fb3b9cd',  # 2022-11-28
    'condition': 'not build_with_chromium',
  },

  'third_party/valijson/src': {
    'url': Var('github') + '/tristanpenman/valijson.git' +
      '@' + '78ac8a737df56b5334354efe104ea8f99e2a2f00', # Version 1.0
    'condition': 'not build_with_chromium',
  },

  # Googleurl recommends living at head. This is a copy of Chrome's URL parsing
  # library. It is meant to be used by QUICHE.
  'third_party/googleurl/src': {
    'url': Var('quiche_git') + '/googleurl.git' +
      '@' + 'dfe8ef6164f8b4e3e9a9cbe8521bb81359918393',  #2023-08-01
    'condition': 'not build_with_chromium',
  }
}

hooks = [
  {
    'name': 'clang_update_script',
    'pattern': '.',
    'condition': 'not build_with_chromium',
    'action': [ 'python3', 'tools/download-clang-update-script.py',
                '--output', 'tools/clang/scripts/update.py' ],
    # NOTE: This file appears in .gitignore, as it is not a part of the
    # openscreen repo.
  },
  {
    'name': 'update_clang',
    'pattern': '.',
    'condition': 'not build_with_chromium',
    'action': [ 'python3', 'tools/clang/scripts/update.py' ],
  },
  {
    'name': 'clang_coverage_tools',
    'pattern': '.',
    'condition': 'not build_with_chromium and checkout_clang_coverage_tools',
    'action': ['python3', 'tools/clang/scripts/update.py',
               '--package=coverage_tools'],
  },
  {
    'name': 'clang_format_linux64',
    'pattern': '.',
    'action': [ 'download_from_google_storage.py', '--no_resume', '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'buildtools/linux64/clang-format.sha1' ],
    'condition': 'host_os == "linux" and not build_with_chromium',
  },
  {
    'name': 'clang_format_mac_x64',
    'pattern': '.',
    'action': [ 'download_from_google_storage.py', '--no_resume', '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'buildtools/mac/clang-format.x64.sha1' ],
    'condition': 'host_os == "mac" and host_cpu == "x64" and not build_with_chromium',
  },
  {
    'name': 'clang_format_mac_arm64',
    'pattern': '.',
    'action': [ 'download_from_google_storage.py', '--no_resume', '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'buildtools/mac/clang-format.arm64.sha1' ],
    'condition': 'host_os == "mac" and host_cpu == "arm64" and not build_with_chromium',
  },
  {
    'name': 'clang_format_win',
    'pattern': '.',
    'action': [ 'download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'buildtools/win/clang-format.exe.sha1',
    ],
    'condition': 'host_os == "win" and not build_with_chromium',
  },
  {
    'name': 'msan_chained_origins_focal',
    'pattern': '.',
    'condition': 'checkout_instrumented_libraries and not build_with_chromium',
    'action': [
                'download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-instrumented-libraries',
                '-s', 'third_party/instrumented_libraries/binaries/msan-chained-origins-focal.tgz.sha1',
              ],
  },
  {
    'name': 'msan_no_origins_focal',
    'pattern': '.',
    'condition': 'checkout_instrumented_libraries and not build_with_chromium',
    'action': [
                'download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-instrumented-libraries',
                '-s', 'third_party/instrumented_libraries/binaries/msan-no-origins-focal.tgz.sha1',
              ],
  },
 {
    'name': 'sysroot_arm',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_arm and not build_with_chromium',
    'action': ['python3', 'build/linux/sysroot_scripts/install-sysroot.py',
               '--sysroots-json-path=' + Var('sysroots_json_path'),
               '--arch=arm'],
  },
  {
    'name': 'sysroot_arm64',
    'pattern': '.',
    'condition': 'not build_with_chromium and checkout_linux and checkout_arm64',
    'action': ['python3', 'build/linux/sysroot_scripts/install-sysroot.py',
               '--sysroots-json-path=' + Var('sysroots_json_path'),
               '--arch=arm64'],
  },
  {
    'name': 'sysroot_x86',
    'pattern': '.',
    'condition': 'not build_with_chromium and checkout_linux and (checkout_x86 or checkout_x64)',
    'action': ['python3', 'build/linux/sysroot_scripts/install-sysroot.py',
               '--sysroots-json-path=' + Var('sysroots_json_path'),
               '--arch=x86'],
  },
  {
    'name': 'sysroot_mips',
    'pattern': '.',
    'condition': 'not build_with_chromium and checkout_linux and checkout_mips',
    'action': ['python3', 'build/linux/sysroot_scripts/install-sysroot.py',
               '--sysroots-json-path=' + Var('sysroots_json_path'),
               '--arch=mips'],
  },
  {
    'name': 'sysroot_mips64',
    'pattern': '.',
    'condition': 'not build_with_chromium and checkout_linux and checkout_mips64',
    'action': ['python3', 'build/linux/sysroot_scripts/install-sysroot.py',
               '--sysroots-json-path=' + Var('sysroots_json_path'),
               '--arch=mips64el'],
  },
  {
    'name': 'sysroot_x64',
    'pattern': '.',
    'condition': 'not build_with_chromium and checkout_linux and checkout_x64',
    'action': ['python3', 'build/linux/sysroot_scripts/install-sysroot.py',
               '--sysroots-json-path=' + Var('sysroots_json_path'),
               '--arch=x64'],
  },
  {
    # Case-insensitivity for the Win SDK. Must run before win_toolchain below.
    'name': 'ciopfs_linux',
    'pattern': '.',
    'condition': 'not build_with_chromium and checkout_win and host_os == "linux"',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/ciopfs',
                '-s', 'build/ciopfs.sha1',
    ]
  },
  {
    # Update the Windows toolchain if necessary.  Must run before 'clang' below.
    'name': 'win_toolchain',
    'pattern': '.',
    'condition': 'not build_with_chromium and checkout_win',
    'action': ['python3', 'build/vs_toolchain.py', 'update', '--force'],
  },
  {
    # Update the Mac toolchain if necessary.
    'name': 'mac_toolchain',
    'pattern': '.',
    'condition': 'not build_with_chromium and (checkout_mac or checkout_ios)',
    'action': ['python3', 'build/mac_toolchain.py'],
  },
  {
    # Update the prebuilt clang toolchain.
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang',
    'pattern': '.',
    'condition': 'not build_with_chromium and not llvm_force_head_revision',
    'action': ['python3', 'tools/clang/scripts/update.py'],
  },
  {
    # Build the clang toolchain from tip-of-tree.
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang_tot',
    'pattern': '.',
    'condition': 'not build_with_chromium and llvm_force_head_revision',
    'action': ['python3', 'tools/clang/scripts/build.py',
               '--llvm-force-head-revision'
               ],
  },
  {
    # This is also supposed to support the same set of platforms as 'clang'
    # above. LLVM ToT support isn't provided at the moment.
    'name': 'clang_tidy',
    'pattern': '.',
    'condition': 'not build_with_chromium and checkout_clang_tidy',
    'action': ['python3', 'tools/clang/scripts/update.py',
               '--package=clang-tidy'],
  },
  {
    # This is also supposed to support the same set of platforms as 'clang'
    # above. LLVM ToT support isn't provided at the moment.
    'name': 'clangd',
    'pattern': '.',
    'condition': 'not build_with_chromium and checkout_clangd',
    'action': ['python3', 'tools/clang/scripts/update.py',
               '--package=clangd'],
  },
  {
    # Update LASTCHANGE.
    'name': 'lastchange',
    'pattern': '.',
    'condition': 'not build_with_chromium',
    'action': ['python3', 'build/util/lastchange.py',
               '-o', 'build/util/LASTCHANGE'],
  },
]

include_rules = [
  '+util',
  '+platform/api',
  '+platform/base',
  '+platform/test',
  '+testing/util',
  '+third_party',

  # Inter-module dependencies must be through public APIs.
  '-discovery',
  '+discovery/common',
  '+discovery/dnssd/public',
  '+discovery/mdns/public',
  '+discovery/public',

  # Don't include abseil from the root so the path can change via include_dirs
  # rules when in Chromium.
  '-third_party/abseil',

  # Abseil allowed headers.
  '+absl/algorithm/container.h',
  '+absl/base/thread_annotations.h',
  '+absl/hash/hash.h',
  '+absl/hash/hash_testing.h',
  '+absl/strings/numbers.h',
  '+absl/strings/str_cat.h',
  '+absl/strings/str_join.h',
  '+absl/strings/str_replace.h',
  '+absl/strings/str_split.h',
  '+absl/strings/substitute.h',
  '+absl/synchronization/mutex.h',
  '+absl/types/variant.h',

  # Similar to abseil, don't include boringssl using root path.  Instead,
  # explicitly allow 'openssl' where needed.
  '-third_party/boringssl',

  # Test framework includes.
  "-third_party/googletest",
  "+gtest",
  "+gmock",
]
