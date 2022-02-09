#!/usr/bin/env python
# Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved
"""
This script stores various script and resource files from a given directory into
a json bundle, which is a convenient way to package them for android assets or
other uses where it makes most sense to load them all at once.
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

    parser.add_argument('--input_dir', type=valid_dir, required=True)
    parser.add_argument('--output', type=argparse.FileType('w'), required=True)
    parser.add_argument('--exclude', nargs='*', default=[])
    parser.add_argument('--add-empty', nargs='*', default=[])
    args = parser.parse_args()

    files = [f
        for f in os.listdir(args.input_dir)
        if os.path.isfile(os.path.join(args.input_dir, f)) and
            f not in args.exclude]

    def get_content(filepath):
        with open(filepath, mode='rb') as file:
            """
            When used to package ublock resources, the packaged Javascript files
            can be used for script injection which requires a non-base64-encoded
            format. So we generalize and don't try to encode files that are
            trusted to be plain text
            """
            if os.path.splitext(filepath)[1] in [
                '.txt', '.html', '.js', '.css' ]:
                    return file.read().decode("utf-8")
            return base64.b64encode(file.read()).decode("utf-8")

    contents = { name: get_content(os.path.join(args.input_dir, name))
        for name in files}

    for e in args.add_empty:
        contents[e] = ""

    json.dump(contents, args.output)

if __name__ == "__main__":
    main()