// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extraparts/vivaldi_unit_test_suite.h"

#include "extensions/vivaldi_extensions_client.h"

VivaldiChromeUnitTestSuite::VivaldiChromeUnitTestSuite(int argc, char** argv)
 : ChromeUnitTestSuite(argc, argv){
  extensions::VivaldiExtensionsClient::RegisterVivaldiExtensionsClient();
}

VivaldiChromeUnitTestSuite::~VivaldiChromeUnitTestSuite() {}
