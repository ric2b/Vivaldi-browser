// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_MAC_H_
#define EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_MAC_H_

#include "build/build_config.h"
#include "extraparts/vivaldi_browser_main_extra_parts.h"

class VivaldiBrowserMainExtraPartsMac : public VivaldiBrowserMainExtraParts {
 public:
  VivaldiBrowserMainExtraPartsMac();
  ~VivaldiBrowserMainExtraPartsMac() override;
  VivaldiBrowserMainExtraPartsMac(const VivaldiBrowserMainExtraPartsMac&) =
      delete;
  VivaldiBrowserMainExtraPartsMac& operator=(
      const VivaldiBrowserMainExtraPartsMac&) = delete;

  // Overridden from ChromeBrowserMainExtraParts:
  void PostEarlyInitialization() override;
  void PreProfileInit() override;
};

#endif  // EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_MAC_H_
