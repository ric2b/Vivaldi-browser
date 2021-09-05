#!/usr/bin/env python
# Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved
"""
This script packages various resources from the ublock repository to integrate
into the vivaldi package. The content of the output json file is simply a
dictionary mapping the original file name to the content of the file. The
content is base-64 encoded, except for non-text formats.
"""

import argparse
import base64
import json
import os
import os.path

def main():
    """main script code"""
    parser = argparse.ArgumentParser(
        description='Packages various resources from ublock to the format used by vivaldi')

    def valid_dir(dir):
        if not os.path.isdir(dir):
            raise argparse.ArgumentTypeError("%s is not a valid directory" % dir)
        return dir

    parser.add_argument('--ublock_dir', type=valid_dir, required=True)
    parser.add_argument('--output', type=argparse.FileType('w'), required=True)
    args = parser.parse_args()

    res_path = os.path.join(args.ublock_dir, 'src', 'web_accessible_resources')

    files = [f
        for f in os.listdir(res_path)
        if os.path.isfile(os.path.join(res_path, f)) and f not in [
            "README.txt", "empty"]]

    def get_content(filepath):
        with open(filepath, mode='rb') as file:
            """
            Javascript files can be used either for redirection or for
            script injection and the latter requires a non-base64-encoded
            format.
            """
            if  os.path.splitext(filepath)[1] in [ '.txt', '.html', '.js' ]:
                return file.read()
            return base64.b64encode(file.read())

    contents = { name: get_content(os.path.join(res_path, name))
        for name in files}

    #Adding this to support the blank-css rewrite from adblock rules
    contents['noop.css'] = ""

    json.dump(contents, args.output)

if __name__ == "__main__":
    main()