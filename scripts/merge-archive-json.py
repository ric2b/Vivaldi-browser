
import argparse
import json


parser = argparse.ArgumentParser()

parser.add_argument("output")
parser.add_argument("chromium")
parser.add_argument("files", nargs=argparse.REMAINDER)

options = parser.parse_args()

output_target = {}

with open(options.chromium, "tr") as f:
  chromium_json = json.load(f)

for x in chromium_json["archive_datas"]:
  new_entry = dict([(i,x[i]) for i in ["files", "file_globs", "archive_type", "dirs"] if i in x])
  if x["archive_type"] == "ARCHIVE_TYPE_ZIP":
    if "rename_dirs" not in x:
      continue
    target_name = x["rename_dirs"][0]["to_dir"]
  elif "files" in x:
    target_name = x["files"][0]
  elif "file_globs" in x:
    target_name = x["file_globs"][0]
    new_entry["name_after_glob"]=True
  else:
    continue # ignore if no name can be determined

  output_target[target_name] = new_entry

for fname in options.files:
  with open(fname, "tr") as f:
    mod_list = json.load(f)
  keep_targets = mod_list.pop("keep", None)
  remove_targets = mod_list.pop("remove", None)
  remove_files = mod_list.pop("remove-files", None)
  remove_fileglobs = mod_list.pop("remove-fileglobs", None)
  if keep_targets or keep_targets == []:
    for x in  list(output_target.keys()):
      if x not in keep_targets:
        del output_target[x]
  elif remove_targets:
    for x in remove_targets:
      if x in output_target:
        del output_target[x]
  if remove_files:
    for y in output_target.values():
      if "files" in y:
        for x in remove_files:
          if x in y["files"]:
            y["files"].remove(x)
  if remove_fileglobs:
    for y in output_target.values():
      if "file_globs" in y:
        for x in remove_fileglobs:
          if x in y["file_globss"]:
            y["files_globs"].remove(x)

  for x,y in mod_list.items():
    if x in output_target:
      for i,j in y.items():
        if i in output_target[x]:
          if isinstance(j, list):
            output_target[x][i].extend(j)
          else:
            output_target[x][i] = j
        else:
          output_target[x][i] = j
    elif not y or (len(y) == 1 and "versioned" in y):
      output_target[x] = y | {
        "files":[x],
        "archive_type": "ARCHIVE_TYPE_FILES",
        }
    else:
     output_target[x] = y

with open(options.output, "tw") as f:
  json.dump(output_target, f, indent=2)
