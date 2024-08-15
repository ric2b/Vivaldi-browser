#!/usr/bin/env python
# Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

import argparse
import json


def get_name(declaration):
    (name, _, definition) = declaration.strip().partition(':')

    if definition == '':
        return f'"{name}"'
    return name

def main():
    parser = argparse.ArgumentParser(
        description='Make ABP snippets directly injectable')

    parser.add_argument('--snippets_bundle', type=argparse.FileType('r'),
                        required=True)
    parser.add_argument('--output', type=argparse.FileType('w'),
                        required=True)
    args = parser.parse_args()

    snippets_bundle = args.snippets_bundle.read()
    (_, _, remainder) = snippets_bundle.partition('const snippets = {')
    (snippets_block, _, _) = remainder.partition('};')

    snippets_declarations = snippets_block.split(',')
    snippets_list = [
        get_name(declaration) for declaration in snippets_declarations]

    args.output.write(",\n".join(snippets_list))

if __name__ == "__main__":
    main()
