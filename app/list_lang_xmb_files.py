import sys

filename_base = sys.argv[1]

for lang in sys.argv[2:]:
  print sys.argv[1] + '_'+ lang+ '.xmb'