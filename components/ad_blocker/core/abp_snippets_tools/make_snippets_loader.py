#!/usr/bin/env python
# Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

import argparse
import json


def main():
    parser = argparse.ArgumentParser(
        description='Make ABP snippets directly injectable')

    parser.add_argument('--snippets_code', type=argparse.FileType('r'),
                        required=True)
    parser.add_argument('--output', type=argparse.FileType('w'),
                        required=True)
    args = parser.parse_args()
    args.output.write('('+ args.snippets_code.read() +')({}, {{1}})')

if __name__ == "__main__":
    main()
