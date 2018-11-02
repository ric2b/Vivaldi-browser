// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_MAC_H_
#define EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_MAC_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "extraparts/vivaldi_browser_main_extra_parts.h"
#include <string>

class VivaldiBrowserMainExtraPartsMac : public VivaldiBrowserMainExtraParts {
 public:
  VivaldiBrowserMainExtraPartsMac();
  ~VivaldiBrowserMainExtraPartsMac() override;

  // Overridden from ChromeBrowserMainExtraParts:
  void PostEarlyInitialization() override;
  void PreProfileInit() override;

  bool checkVersionPath(std::string location,
                        std::string pepperPath,
                        std::string* versionPath);

  DISALLOW_COPY_AND_ASSIGN(VivaldiBrowserMainExtraPartsMac);
};

#endif  // EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_MAC_H_
