# Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

import  os
import argparse
import sys
import refresh_file

def DoMain(argv):
  argparser = argparse.ArgumentParser();
  argparser.add_argument("main_file")
  argparser.add_argument("target_file")
  argparser.add_argument("resourcefile")
  argparser.add_argument("newresourcefile")
  argparser.add_argument("sourcedirpath")

  options = argparser.parse_args(argv)

  with open(options.main_file,"r") as idfile:
    main_file = eval(idfile.read())
    assert(isinstance(main_file, dict))

  resource_base = os.path.abspath(os.path.join(
            os.path.dirname(options.main_file), main_file["SRCDIR"]))
  rel_resource_file = os.path.relpath(
            options.resourcefile, resource_base).replace("\\", "/")
  rel_new_resource = os.path.relpath(options.newresourcefile,
                            os.path.dirname(resource_base)).replace("\\", "/")

  new_dict = {
    "SRCDIR":options.sourcedirpath,
    rel_new_resource: main_file[rel_resource_file]
  }

  try:
    os.makedirs(os.path.dirname(options.target_file))
  except:
    # Just ignore the errors here. Most likely the dir exists,
    # otherwise next step will fail instead
    pass
  refresh_file.conditional_refresh_file(options.target_file, str(new_dict))

if __name__ == '__main__':
    DoMain(sys.argv[1:])
