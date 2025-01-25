
import os
import subprocess
import sys
import vivdeps

from git_urls import BASE_URL, CHROMIUM_URL

SRC = os.path.dirname(os.path.dirname(__file__))

cipd_pick_list = [
  "tools/clang/dsymutil",
  "third_party/devtools-frontend/src/third_party/esbuild",
  "buildtools/reclient",
  "third_party/lzma_sdk/bin/host_platform",
  "third_party/lzma_sdk/bin/win64",
  "ui/gl/resources/angle-metal",
  "build/linux/debian_bullseye_amd64-sysroot",
  "build/linux/debian_bullseye_arm64-sysroot",
  "build/linux/debian_bullseye_armhf-sysroot",
  "build/linux/debian_bullseye_i386-sysroot",
  "third_party/test_fonts",
  "third_party/subresource-filter-ruleset/data",
  "third_party/node/node_modules",
  "third_party/rust-toolchain",
  "third_party/llvm-build/Release+Asserts",
  "buildtools/win-format",
  "buildtools/mac-format",
  "buildtools/mac_arm64-format",
  "buildtools/linux64-format",
  "third_party/test_fonts/test_fonts",
  ]

exclude_cipd = [
  "buildtools/linux64",
  "buildtools/mac",
  "buildtools/win",
  "tools/skia_goldctl/linux",
  "tools/skia_goldctl/win",
  "tools/skia_goldctl/mac",
  "tools/luci-go",
  "tools/resultdb",
  ]

excluded_modules = [
  "third_party/cros-components/src",
  "third_party/cardboard/src",
  "third_party/libFuzzer/src",
  "third_party/chromium-variations",
  "third_party/speedometer/v3.0",
  "third_party/google-truth/src",
  "testing/libfuzzer/fuzzers/wasm_corpus",
  "third_party/crabbyavif/src",
  "third_party/instrumented_libs",
  "third_party/domato/src",
  "tools/page_cycler/acid3",
  "third_party/webdriver/pylib",
  ]

def main():
  variables = vivdeps.get_chromium_variables()
  if variables.get("checkout_android", False):
    variables["checkout_android_native_support"] = True

  deps = vivdeps.ChromiumDeps(variables=variables)

  deps.Load(recurse=True)

  existing_submodules = []
  only_cipd = True
  pick_list = cipd_pick_list

  if variables.get("checkout_android", False) or variables.get("checkout_ios", False):
    only_cipd = False
    pick_list = None

    result = subprocess.run(["git", "submodule", "status", "--recursive"],
                            capture_output=True,
                            check=True,
                            text=True,
                            cwd=os.path.join(SRC, "chromium"))

    existing_submodules = [x.split()[1] for x in result.stdout.splitlines()]

  deps.UpdateModules(pick_list, existing_submodules + excluded_modules, only_cipd, exclude_cipd)

if __name__ == '__main__':
  sys.exit(main())