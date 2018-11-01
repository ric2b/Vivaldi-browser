// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef TESTING_DISABLE_UNITTESTS_MACROS_H_
#define TESTING_DISABLE_UNITTESTS_MACROS_H_

#define DISABLE(testcase, testname) {#testcase, #testname, false},
#define DISABLE_ALL(testcase) {#testcase, NULL, false},
#define DISABLE_MULTI(testcase, testname) {#testcase, #testname, true},

#endif  // TESTING_DISABLE_UNITTESTS_MACROS_H_
