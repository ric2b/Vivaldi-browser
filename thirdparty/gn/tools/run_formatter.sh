#!/bin/sh

cd $(dirname $(dirname $0))

ensure_file=$(mktemp)

# https://chrome-infra-packages.appspot.com/p/fuchsia/third_party/clang
echo 'fuchsia/third_party/clang/${platform} integration' > $ensure_file
cipd ensure -ensure-file $ensure_file -root clang

git ls-files | egrep '\.(h|cc)$' | fgrep -v 'third_party' |\
    xargs ./clang/bin/clang-format -i
