
import os
import sys

if len(sys.argv) == 2 and os.access(sys.argv[1], os.F_OK):
  print("true")
else:
  print("false")
