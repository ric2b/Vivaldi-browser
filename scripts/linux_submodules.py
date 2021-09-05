
import os
import sys
import subprocess

import fetch_submodules

SRC = os.path.dirname(os.path.dirname(__file__))

def cipd_only_filter(submodules):
  if "__cipd__" in submodules:
    return {"__cipd__": submodules["__cipd__"]}
  return {}

def main():
  fetch_submodules.GetSubmodules("checkout_linux", checkout_filter=cipd_only_filter)
  return 0

if __name__ == '__main__':
  sys.exit(main())