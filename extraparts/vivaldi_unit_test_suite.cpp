// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extraparts/vivaldi_unit_test_suite.h"

#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/vivaldi_extensions_client.h"
#endif

VivaldiChromeUnitTestSuite::VivaldiChromeUnitTestSuite(int argc, char** argv)
    : ChromeUnitTestSuite(argc, argv) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::VivaldiExtensionsClient::RegisterVivaldiExtensionsClient();
#endif
}

VivaldiChromeUnitTestSuite::~VivaldiChromeUnitTestSuite() {}
