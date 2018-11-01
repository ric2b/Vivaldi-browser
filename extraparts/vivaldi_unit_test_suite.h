// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTRAPARTS_VIVALDI_UNIT_TEST_SUITE_H_
#define EXTRAPARTS_VIVALDI_UNIT_TEST_SUITE_H_

#include "chrome/test/base/chrome_unit_test_suite.h"

class VivaldiChromeUnitTestSuite : public ChromeUnitTestSuite {
 public:
  VivaldiChromeUnitTestSuite(int argc, char** argv);
  ~VivaldiChromeUnitTestSuite() override;
};

#endif  // EXTRAPARTS_VIVALDI_UNIT_TEST_SUITE_H_
