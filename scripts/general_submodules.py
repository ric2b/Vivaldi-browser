
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
  ]

def main():
  variables = vivdeps.get_chromium_variables()
  if variables.get("checkout_android", False):
    variables["checkout_android_native_support"] = True

  # Temp: Freeze reclient version to 112, due to breakage in 114
  variables["reclient_version"] = "re_client_version:0.96.2.d36a87c-gomaip"

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