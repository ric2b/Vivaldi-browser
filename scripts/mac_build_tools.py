
import sys
import platform
import fetch_submodules

def main():
  if platform.system() != "Darwin":
    return 0

  def filter_dict(a_dict):
    module_list = ["tools/clang/dsymutil"]
    result = {}
    for x, y in a_dict.items():
      if x == "__cipd__":
        result[x] = filter_dict(y)
      elif x in module_list:
        result[x] = y
    return result

  fetch_submodules.GetSubmodules("checkout_mac", filter_dict)

  return 0

if __name__ == '__main__':
  sys.exit(main())