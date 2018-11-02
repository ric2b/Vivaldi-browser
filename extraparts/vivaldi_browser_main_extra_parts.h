// Copyright (c) 2015-2018 Vivaldi Technologies AS. All rights reserved

#ifndef EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_H_
#define EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"

class VivaldiBrowserMainExtraParts : public ChromeBrowserMainExtraParts {
 public:
  VivaldiBrowserMainExtraParts();
  ~VivaldiBrowserMainExtraParts() override;

  // Overridden from ChromeBrowserMainExtraParts:
  void PostEarlyInitialization() override;
  void PreProfileInit() override;
  void PostProfileInit() override;

  static VivaldiBrowserMainExtraParts* Create();
 private:
  void EnsureBrowserContextKeyedServiceFactoriesBuilt();

  DISALLOW_COPY_AND_ASSIGN(VivaldiBrowserMainExtraParts);
};

#endif  // EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_H_
