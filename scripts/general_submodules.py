
import os
import sys
import subprocess

import fetch_submodules

SRC = os.path.dirname(os.path.dirname(__file__))

OS_CHOICES = {
  "win32": "win",
  "win": "win",
  "cygwin": "win",
  "darwin": "mac",
  "ios": "ios",
  "mac": "mac",
  "unix": "linux",
  "linux": "linux",
  "linux2": "linux",
  "linux3": "linux",
  "android": "android",
  "arm":"arm",
  "arm64":"arm64",
  "x86":"x86",
  "x64":"x64",
}

cipd_pick_list = [
  "tools/clang/dsymutil",
  ]

def IsAndroidEnabled():
  if "ANDROID_ENABLED" in os.environ:
    return True
  return os.access(os.path.join(SRC,".enable_android"), os.F_OK)

host_os = OS_CHOICES.get(sys.platform, 'unix')
checkout_os = ["checkout_"+host_os]

if IsAndroidEnabled():
  checkout_os = ["checkout_android", "checkout_android_native_support"]

def cipd_only_filter(submodules):
  if "__cipd__" in submodules:
    accept_conditions = any([x in ["checkout_android", "checkout_linux"] for x in checkout_os])
    picked_modules = {}
    for x, d in submodules["__cipd__"].items():
      if x in cipd_pick_list or (accept_conditions and "condition" in d):
        picked_modules[x] = {"packages": d.get("packages", [])}
    return {"__cipd__": picked_modules}
  return {}

def main():
  fetch_submodules.GetSubmodules(checkout_os, checkout_filter=cipd_only_filter if "checkout_android" not in checkout_os else None)
  return 0

if __name__ == '__main__':
  sys.exit(main())