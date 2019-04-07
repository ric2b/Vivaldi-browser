
import os
import sys
import subprocess

import fetch_submodules

SRC = os.path.dirname(os.path.dirname(__file__))

def IsAndroidEnabled():
  if "ANDROID_ENABLED" in os.environ:
    return True
  return os.access(os.path.join(SRC,".enable_android"), os.F_OK)

def main():
  if not IsAndroidEnabled():
    return 0

  fetch_submodules.GetSubmodules("checkout_android")
  return 0

if __name__ == '__main__':
  sys.exit(main())