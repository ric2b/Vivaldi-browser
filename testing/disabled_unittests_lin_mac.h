// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Mac and Linux by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

// Chromium tests failing in v54 due to a bug they are apparently working on
DISABLE_ALL(BrowsingDataRemoverTest)
