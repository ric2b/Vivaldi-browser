#! /bin/bash

set -e

CC=${CC:-gcc}

case "$1" in
  *.exe)
    export WINEPATH=$(${CC} -print-sysroot)/mingw/bin
    fccwd=`pwd`
    cd $(IFS=:;for i in $PATH; do echo $i|grep mingw> /dev/null; [ $? -eq 0 ] && echo $i; done)
    if [ "x$(dirname $@)" = "x." ]; then
        /usr/bin/env wine $fccwd/$@
    else
        /usr/bin/env wine $@
    fi
    ;;
  *)
    $@
    ;;
esac

