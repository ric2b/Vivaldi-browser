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
  'gn_version': 'git_revision:487f8353f15456474437df32bb186187b0940b45',
  'clang_format_revision': '37f6e68a107df43b7d7e044fd36a13cbae3413f2',

  # Chrome version to pull clang update.py script from. This is necessary
  # because this script does experience breaking changes, such as removing
  # command line arguments, that need to be handled intentionally by a roll.
  'chrome_version': 'ddf8f8a465be2779bd826db57f1299ccd2f3aa25',

  # 'magic' text to tell depot_tools that git submodules should be accepted
  # but parity with DEPS file is expected.
  'SUBMODULE_MIGRATION': 'True',

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
      '@' + '077a66f30fcf281b066fafb6dfc60818c238efb6',
  },

  # and here:
  # https://chromium.googlesource.com/chromium/src/build/+/refs/heads/main
  'build': {
    'url': Var('chromium_git') + '/chromium/src/build' +
      '@' + '526c6cbd6a32e5952e0cc09ab35d33ea4d883907',
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
        'version': 'version:2@1.12.1.chromium.4',
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
      '@' + '51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf', # version 1.3.1
    'condition': 'not build_with_chromium',
  },

  'third_party/jsoncpp/src': {
    'url': Var('chromium_git') +
      '/external/github.com/open-source-parsers/jsoncpp.git' +
      '@' + '89e2973c754a9c02a49974d839779b151e95afd6', # version 1.9.6
    'condition': 'not build_with_chromium',
  },

  # googletest now recommends "living at head," which is a bit of a crapshoot
  # because regressions land upstream frequently.  This is a known good revision.
  'third_party/googletest/src': {
    'url': Var('chromium_git') +
      '/external/github.com/google/googletest.git' +
      '@' + 'eb2d85edd0bff7a712b6aff147cd9f789f0d7d0b',  # 2025-08-28
    'condition': 'not build_with_chromium',
  },

  'third_party/boringssl/src': {
    'url' : Var('boringssl_git') + '/boringssl.git' +
      '@' + '26e8a8acb91a0cfbd2f95bf7245e2eb87d533a2f',
    'condition': 'not build_with_chromium',
  },

  # To roll forward, use quiche_revision from chromium/src/DEPS.
  'third_party/quiche/src': {
    'url': Var('quiche_git') + '/quiche.git' +
      '@' + '5a433bd7de22c23700d046346bd3d3afe5c9cd07',  # 2025-02-10
    'condition': 'not build_with_chromium',
  },

  'third_party/instrumented_libs': {
    'url': Var('chromium_git') + '/chromium/third_party/instrumented_libraries.git' +
      '@' + '3cc43119a29158bcde39d288a8def4b8ec49baf8',
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
      'dd4c89bd657f1e247ce5111a5c89ffe6ccfd0c92',  # 2025-01-30
    'condition': 'not build_with_chromium',
  },

  'third_party/libfuzzer/src': {
    'url': Var('chromium_git') +
      '/external/github.com/llvm/llvm-project/compiler-rt/lib/fuzzer.git' +
      '@' + 'e31b99917861f891308269c36a32363b120126bb',
    'condition': 'not build_with_chromium',
  },

  'third_party/libc++/src': {
    'url': Var('chromium_git') +
    '/external/github.com/llvm/llvm-project/libcxx.git' + '@' + '11c38d901d29bc91aee3efb53652f7141f72f47f',
    'condition': 'not build_with_chromium',
  },

  'third_party/libc++abi/src': {
    'url': Var('chromium_git') +
    '/external/github.com/llvm/llvm-project/libcxxabi.git' + '@' + '83dfa1f5bfce32d5f75695542468e37ead8163b8',
    'condition': 'not build_with_chromium',
  },

  'third_party/llvm-libc/src': {
    'url': Var('chromium_git') +
      '/external/github.com/llvm/llvm-project/libc.git' + '@' + '2222607a3ea3d5f65338d3b36a4cc5fb563169ab',
    'condition': 'not build_with_chromium',
  },

  'third_party/modp_b64': {
    'url': Var('chromium_git') + '/chromium/src/third_party/modp_b64'
    '@' + '7c1b3276e72757e854b5b642284aa367436a4723',  # 2024-11-18
    'condition': 'not build_with_chromium',
  },

  'third_party/valijson/src': {
    'url': Var('github') + '/tristanpenman/valijson.git' +
      '@' + 'fc9ddf14db683c9443c48ae3a6bf83e0ce3ad37c', # Version 1.0.3
    'condition': 'not build_with_chromium',
  },

  # Googleurl recommends living at head. This is a copy of Chrome's URL parsing
  # library. It is meant to be used by QUICHE.
  #
  # NOTE: Pin to the current revision as newer versions use C++20 features.
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
                '--revision', Var('chrome_version'),
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
]

# This exists to allow Google Cloud Storage blobs in these DEPS to be fetched.
# Do not add any additional recursedeps entries without consulting
# mfoltz@chromium.org!
recursedeps = [
  'build',
  'buildtools',
  'third_party/instrumented_libs',
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
  # IMPORTANT: Do not add new entries; abseil is being removed from the library.
  # See https://issuetracker.google.com/158433927
  '+absl/types/variant.h',

  # Similar to abseil, don't include boringssl using root path.  Instead,
  # explicitly allow 'openssl' where needed.
  '-third_party/boringssl',

  # Test framework includes.
  '-third_party/googletest',
  '+gtest',
  '+gmock',

  # Can use generic Chromium buildflags.
  '+build/build_config.h',
]
