#!/bin/bash

set -x

# Enter build directory.
rootdir=`pwd`
cd ${EIGEN_CI_BUILDDIR}

target=""
if [[ ${EIGEN_CI_CTEST_REGEX} ]]; then
  target="-R ${EIGEN_CI_CTEST_REGEX}"
elif [[ ${EIGEN_CI_CTEST_LABEL} ]]; then
  target="-L ${EIGEN_CI_CTEST_LABEL}"
fi

# Repeat tests up to three times to ignore flakes.  Do not re-run with -T test,
# otherwise we lose test results for those that passed.
# Note: starting with CMake 3.17, we can use --repeat until-pass:3, but we have
#       no way of easily installing this on ppc64le.
ctest ${EIGEN_CI_CTEST_ARGS} --parallel ${NPROC} \
      --output-on-failure --no-compress-output  \
      --build-no-clean -T test ${target} || \
  ctest ${EIGEN_CI_CTEST_ARGS} --parallel ${NPROC} \
      --output-on-failure --no-compress-output --rerun-failed || \
  ctest ${EIGEN_CI_CTEST_ARGS} --parallel ${NPROC} \
      --output-on-failure --no-compress-output --rerun-failed

# Return to root directory.
cd ${rootdir}

set +x
