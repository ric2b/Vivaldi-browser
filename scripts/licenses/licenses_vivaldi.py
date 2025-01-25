from subprocess import check_output
import re
import json
from os.path import exists, join, normpath
from licenses_vivaldi_texts import onlineLicenses

try:
  f = open(normpath("gen/vivaldi/vivapp/module_list"), mode="r", encoding="utf-8")
  maindeps = f.read()
  f.close()
except:
  maindeps = ""

modules = {}

module_regexp = re.compile(r"(.*node_modules[/\\]((?:@[^/\\]+[/\\])?[^@/\\][^/\\]+))")

for mline in maindeps.splitlines():
  m = module_regexp.search(mline)
  if not m:
    continue
  moduledir = m[1]
  modulename = m[2]

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
  }

  # get license file (in order of preference)
  for l in ["LICENSE-MIT", "LICENSE-MIT.TXT", "LICENSE.MIT", "LICENSE.BSD",
      "LICENSE.APACHE2", "LICENSE", "LICENSE.txt", "LICENSE.md", "License",
      "license.txt", "License.md", "LICENSE.mkd", "UNLICENSE"]:
    file_name = join(moduledir, l)
    if exists(file_name):
      entry["file_name"] = file_name
      f = open(file_name, mode="r", encoding="utf-8")
      entry["license"] = f.read()
      f.close()
      break

  # get one word license type from package.json
  f = open(join(moduledir, "package.json"), mode="r", encoding="utf-8")
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
  if "licensetype" in entry and entry["licensetype"] not in [
    "(BSD-3-Clause AND Apache-2.0)",
    "(MIT AND Zlib)",
    "Apache 2.0",
    "Apache License, Version 2.0",
    "Apache-2.0",
    "BSD-2-Clause",
    "BSD-3-Clause",
    "BSD",
    "0BSD",
    "CC0-1.0",
    "Creative Commons Attribution 2.5 License",
    "ISC",
    "MIT AND ISC",
    "MIT Licensed. http://www.opensource.org/licenses/mit-license.php",
    "MIT",
    "(MPL-2.0 OR Apache-2.0)",
    "MPL",
    "Public Domain",
    "Unlicense",
    "WTFPL"
  ]:
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
      entry[e] = entry[e]
    except:
      pass
  modules[moduledir] = entry

SPECIAL_CASES = {
    join('..', 'platform_media'): {
        "Name": "Opera",
        "URL": "http://www.opera.com/",
        "License": "BSD",
        "License File": ["/../platform_media/OPERA_LICENSE.txt"],
    },
    join('..', 'update_notifier/', 'thirdparty', 'winsparkle'): {
        "Name": "WinSparkle",
        "URL": "http://winsparkle.org/",
        "License": "MIT",
        "License File": ["/../update_notifier/thirdparty/winsparkle/COPYING"],
    },
    join('..', 'update_notifier/', 'thirdparty', 'wxWidgets'): {
        "Name": "wxWidgets",
        "URL": "https://www.wxwidgets.org/",
        "License": "Modified LGPL",
        "License File": ["/../update_notifier/thirdparty/wxWidgets/docs/licence.txt"],
    },
    join('..', 'thirdparty', 'macsparkle'): {
        "Name": "Sparkle",
        "URL": "http://sparkle-project.org/",
        "License": "MIT",
        "License File": ["/../thirdparty/macsparkle/LICENSE"],
    },
    join('..', 'vivapp', 'src', 'browserjs'): {
        "Name": "boss-select",
        "URL": "https://github.com/grks/boss-select#readme",
        "License": "MIT",
        "License File": ["/../vivapp/src/browserjs/boss-select-license.txt"],
    },
    join('..', 'vivapp', 'src', 'components', 'image-inspector'): {
        "Name": "Exif.js",
        "URL": "https://github.com/exif-js/exif-js",
        "License": "MIT",
        "License File": ["/../vivapp/src/components/image-inspector/exif_js_license.txt"],
    },
    join('..', 'scripts', 'licenses'): {
        "Name": "Profile avatar illustrations",
        "URL": "https://www.flaticon.com/",
        "License": "attribution required",
        "License File": ["/../scripts/licenses/avatar_profiles_license.txt"],
    },
    join('..', 'vivapp', 'src', 'util'): {
        "Name": "Boyer-Moore-Horspool Search",
        "URL": "https://github.com/Chocobo1/bmhs",
        "License": "BSD-3-Clause",
        "License File": ["/../vivapp/src/util/bmhs_license.txt"],
    },
    join('..', 'vivapp', 'src', 'dispatcher'): {
        "Name": "Flux",
        "URL": "https://facebook.github.io/flux/",
        "License": "BSD",
        "License File": ["/../vivapp/fork_licenses/flux/LICENSE"],
    },
    join('..', 'vivapp', 'src', 'forked_modules', 'react-motion'): {
        "Name": "react-motion",
        "URL": "https://github.com/chenglou/react-motion",
        "License": "MIT",
        "License File": ["/../vivapp/src/forked_modules/react-motion/LICENSE"],
    },
    join('..', 'scripts', 'licenses', 'geonames'): {
        "Name": "GeoNames",
        "URL": "https://www.geonames.org",
        "License": "CC BY 4.0",
        "License File": ["/../scripts/licenses/geonames_license.txt"],
    },
}

ADDITIONAL_PATHS = tuple(SPECIAL_CASES.keys())

def GetEntries(entry_template, EvaluateTemplate):
  entries = []
  for module, license in modules.items():
    entries.append({
        'name': license['name'],
        'content': EvaluateTemplate(entry_template, license),
        'license_file': [] if not 'file_name' in license else [license['file_name']],
        })
  return entries

def RemoveEntries(entries):
  # AUTO-208: Remove some license statements of code not used in release builds
  return [
    entry for entry in entries
       # From third_party/subresource-filter-ruleset/README.chromium:
       # "This copy is included in WebLayer releases, but is not used for Chrome."
    if entry['name'] != 'EasyList' and
       # Only referenced in ChromeOS build files
       entry['name'] != 'Braille Translation Library'
  ]
