#!/usr/bin/env python

from subprocess import check_output
import re
import json
import os.path
from licenses_vivaldi_texts import onlineLicenses

basepath = "../../vivapp"
maindeps = check_output(["python", basepath+"/bin/list_jsdeps.py", basepath])

modules = {}

for m in re.findall(r"(.*node_modules/([^/]+))", maindeps):
  moduledir = m[0]
  modulename = m[1]
  if (moduledir in modules
     or (modulename == "chrome") # ours
     or (modulename == "wo-stringencoding") # ours
     or modulename == "vivaldi-color" # ours
     or (modulename == "url") # ignore for now, LICENSE file is on master, https://github.com/defunctzombie/node-url
     or (modulename == "ripemd160") # ignore for now, LICENSE file is on master, https://github.com/cryptocoinjs/ripemd160
     or (modulename == "indexof") # trivial
     or (modulename == "binary-search") # CC0-1.0, no need to put in credits file
    ):
      continue
  entry = {
      "name": modulename,
      "License File": "", # Can't be None due to string conversion below
  }

  # get license file (in order of preference)
  for l in ["LICENSE-MIT", "LICENSE-MIT.TXT", "LICENSE.MIT", "LICENSE.BSD",
      "LICENSE.APACHE2", "LICENSE", "LICENSE.txt", "LICENSE.md", "License",
      "license.txt", "License.md", "LICENSE.mkd", "UNLICENSE"]:
    file_name = basepath+"/"+moduledir+"/"+l
    if os.path.exists(file_name):
      entry["License File"] = file_name
      f = open(file_name)
      entry["license"] = f.read()
      f.close()
      break

  # get one word license type from package.json
  f = open(basepath+"/"+moduledir+"/package.json")
  pjson = json.loads(f.read())
  f.close()
  preferred = None
  if "license" in pjson:
    preferred = "license"
  elif "licence" in pjson: # typo in react-list
    preferred = "licence"
  elif "licenses" in pjson:
    preferred = "licenses"

  if preferred:
    if type(pjson[preferred]) is list:
      entry["licensetype"] = pjson[preferred][0]["type"]
      entry["licenseurl"] = pjson[preferred][0]["url"]
    elif type(pjson[preferred]) is dict:
      entry["licensetype"] = pjson[preferred]["type"]
      entry["licenseurl"] = pjson[preferred]["url"]
    else:
      entry["licensetype"] = pjson[preferred]
  if "licensetype" in entry and entry["licensetype"] not in ["MIT", "BSD",
      "Apache 2.0", "Apache-2.0", "Apache License, Version 2.0", "Creative Commons Attribution 2.5 License", "MPL",
      "BSD-2-Clause", "BSD-3-Clause", "ISC", "MPL-2.0 OR Apache-2.0", "WTFPL", "Public Domain", "CC0-1.0"]:
    print("ERROR: " + moduledir + " uses a license that hasn't been reviewed for Vivaldi: " + entry["licensetype"])
    exit(1)

  if not "license" in entry and "licenseurl" in entry:
    if entry["licenseurl"] in onlineLicenses:
      entry["license"] = onlineLicenses[entry["licenseurl"]]
    else:
      print("ERROR: " + modulename + " provides URL " + entry["licenseurl"] + " as a license but it hasn't been copied to licenses_vivaldi_texts.py")
      exit(1)
  if not "license" in entry and "licensetype" in entry:
    entry["license"] = entry["licensetype"]

  if not "license" in entry:
    print("ERROR: License statement missing for module " + moduledir + ". Add it to the list of exceptions in licenses_vivaldi.py if it's not a third party module.")
    exit(1)

  if "homepage" in pjson:
    entry["url"] = pjson["homepage"]
  else:
    entry["url"] = ("https://www.npmjs.org/package/"+entry["name"])

  if "licensetype" in entry and entry["licensetype"] == "MPL":
    entry["license_unescaped"] = "Source code is available for download at <a href='http://registry.npmjs.org/"+modulename+"/-/"+modulename+"-"+pjson["version"]+".tgz'>http://registry.npmjs.org/"+modulename+"/-/"+modulename+"-"+pjson["version"]+".tgz<a/>. No source code files were modified for use in Vivaldi."
  else:
    entry["license_unescaped"] = ""

  for e in entry:
    try:
      entry[e] = entry[e].encode("ASCII")
    except:
      pass
  modules[moduledir] = entry


ADDITIONAL_PATHS = (
    os.path.join('..', 'third_party', '_winsparkle_lib'),
    os.path.join('..', 'third_party', 'sparkle_lib'),
)

SPECIAL_CASES = {
    os.path.join('..', 'platform_media'): {
        "Name": "Opera",
        "URL": "http://www.opera.com/",
        "License": "BSD",
        "License File": "/../platform_media/OPERA_LICENSE.txt",
    },
    os.path.join('..', 'third_party', '_winsparkle_lib'): {
        "Name": "WinSparkle",
        "URL": "http://winsparkle.org/",
        "License": "MIT",
        "License File": "/../third_party/_winsparkle_lib/COPYING",
    },
    os.path.join('..', 'third_party', 'sparkle_lib'): {
        "Name": "Sparkle",
        "URL": "http://sparkle-project.org/",
        "License": "MIT",
        "License File": "/../third_party/sparkle_lib/LICENSE",
    },
}

def GetEntries(entry_template, EvaluateTemplate):
  entries = []
  for module, license in modules.iteritems():
    entries.append({
        'name': license['name'],
        'content': EvaluateTemplate(entry_template, license),
        'license_file': license['License File'],
        })
  return entries
