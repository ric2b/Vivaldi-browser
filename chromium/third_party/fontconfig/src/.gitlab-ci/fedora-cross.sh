#! /bin/sh

if [ x"$FC_BUILD_PLATFORM" == "xmingw" ]; then
    eval `rpm --eval %{mingw64_env}`
fi
