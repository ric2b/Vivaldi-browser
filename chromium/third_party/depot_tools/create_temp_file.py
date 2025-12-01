#!/usr/bin/env python3
import argparse
import os
import sys
import tempfile


def run(prefix=None, suffix=None):
    """ Reads text from stdin, writes it to a temporary file.

    The path of the temporary file is printed to stdout on success.
    """
    try:
        with tempfile.NamedTemporaryFile(mode='w+',
                                         delete=False,
                                         encoding='utf-8',
                                         prefix=prefix,
                                         suffix=suffix) as temp_file:
            for line in sys.stdin:
                temp_file.write(line)
            temp_file.flush()
            temp_file_path = temp_file.name

        print(temp_file_path)

    except Exception as e:
        print(f"An error occurred: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Writes stdin to a temporary file and then print the path.",
    )
    parser.add_argument('--prefix',
                        help="Optional prefix for the temporary file name.")
    parser.add_argument('--suffix',
                        help="Optional suffix for the temporary file name.")
    args = parser.parse_args()
    run(prefix=args.prefix, suffix=args.suffix)
