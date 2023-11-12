#!/usr/bin/env python
# Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

import argparse
import json


def main():
    parser = argparse.ArgumentParser(
        description='Combines the abp snippets file and its loader into one file')

    parser.add_argument('--loader', type=argparse.FileType('r'),
                        required=True)
    parser.add_argument('--snippets', type=argparse.FileType('r'),
                        required=True)
    parser.add_argument('--output', type=argparse.FileType('w'),
                        required=True)
    args = parser.parse_args()

    content = args.loader.read()
    (copyright_notice, _, _) = content.partition('*/')
    (_, _, remainder) = content.partition('return `\n')
    (loader, _, _) = remainder.partition('`')

    # drop the copyright notice from the snippets file. We preserve the notice
    # for the whole loader + snippets file
    snippets_content = args.snippets.read()
    (_, _, snippets) = snippets_content.partition('*/')

    # This call isn't available outside of extension origins, but is only used
    # in the snippets file to detect firefox, so replacing it by an empty string
    # works.
    snippets = snippets.replace('browser.runtime.getURL("")','""')

    if not loader:
        raise ValueError

    # Sanity check: Make sure that the strings we want to replace are actually
    # there.
    loader.index('${JSON.stringify(libraries)}')
    loader.index('${JSON.stringify([].concat(scripts).map(parseScript))}')
    loader.index('${JSON.stringify(environment)}')

    output = loader.replace(
        '${JSON.stringify([].concat(scripts).map(parseScript))}',
        '{{1}}')
    output = output.replace('${JSON.stringify(environment)}', '{}')
    output = output.replace(
        '${JSON.stringify(libraries)}', json.dumps([snippets]))

    args.output.write(
        copyright_notice + '*/\n' + output)


if __name__ == "__main__":
    main()
