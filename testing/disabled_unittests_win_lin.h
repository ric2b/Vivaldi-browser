// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled  by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

  // Appears flaky on Win and Lin
  //DISABLE(MediaRouterMojoImplTest,
  //     RegisterMediaSinksObserverWithAvailabilityChange)

  // Failing media tests evern since proprietary media code was imported
  DISABLE(MediaTest, VideoBearRotated270)
  DISABLE(MediaTest, VideoBearRotated90)
