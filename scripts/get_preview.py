import sys

assert(len(sys.argv)>1)

branch = sys.argv[1]

branch = branch.split("/")

# Only called for official builds
if len(branch) > 1 and branch[0] == "releases":
  print(branch[1])
elif len(branch) > 1 and branch[0] in [ "snapshot" ]:
  print(branch[0])
elif branch == "main":
  print("snapshot")
else:
  print("normal")
