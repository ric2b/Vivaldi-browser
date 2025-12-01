#! /bin/bash

set -e

ret=0
r=0
echo -n "*** libfontconfig.so installation check: " 1>&2
ret=$(find $PREFIX -name libfontconfig.so | wc -l)
if [ $ret -eq 0 ]; then
  echo "NG - libfontconfig.so not installed." 1>&2
  r=1
else
  echo "OK" 1>&2
fi
echo -n "*** libfontconfig.a installation check: " 1>&2
ret=$(find $PREFIX -name libfontconfig.a | wc -l)
if [ $ret -ne 0 ]; then
  echo "NG - libfontconfig.a installed." 1>&2
  r=1
else
  echo "OK" 1>&2
fi
exit $r
