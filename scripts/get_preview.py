import sys

assert(len(sys.argv)>1)

branch = sys.argv[1]

branch = branch.split("/")

if len(branch) > 1 and branch[0] == "releases":
  print(branch[1])
else:
  print("normal")
