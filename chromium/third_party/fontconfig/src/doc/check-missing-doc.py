#! /usr/bin/env python3
# Copyright (C) 2025 fontconfig Authors
# SPDX-License-Identifier: HPND

import argparse
import glob
import re
import sys
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('srcdir')
parser.add_argument('builddir')

args = parser.parse_args()
h_apis = []
d_apis = []

header_files = glob.glob(str(Path(args.builddir) / 'fontconfig' / '*.h')) + glob.glob(str(Path(args.srcdir) / 'fontconfig' / '*.h'))

for fn in header_files:
    with open(fn) as f:
        for s in f:
            if re.search(r'^Fc', s) and not re.search('FcPublic', s):
                res = re.sub(r'[^a-zA-Z0-9].*', '', s)
                h_apis += [res]
            m = re.search(r'#\s*define\s+(Fc[a-zA-Z]+)\s*\(.*$', s)
            if m:
                h_apis += [m.group(1)]

h_apis.sort()

doc_files = glob.glob(str(Path(args.srcdir) / 'doc' / '*.fncs'))
for fn in doc_files:
    with open(fn) as f:
        for s in f:
            if re.search(r'@FUNC[+]*@', s):
                d_apis += [s.split()[1]]

d_apis.sort()

missing_in_doc = list(set(h_apis) - set(d_apis))
missing_in_header = list(set(d_apis) - set(h_apis))
if len(missing_in_doc) == 0 and len(missing_in_header) == 0:
    sys.exit(0)

if len(missing_in_doc) > 0:
    print('Missing API documentation:')
    for s in missing_in_doc:
        print(f'\t{s}')
    print('')
if len(missing_in_header) > 0:
    print('API available in documentation only:')
    for s in missing_in_header:
        print(f'\t{s}')
    print('')

sys.exit(1)
