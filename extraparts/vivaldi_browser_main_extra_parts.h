// Copyright (c) 2015-2018 Vivaldi Technologies AS. All rights reserved

#ifndef EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_H_
#define EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"

class Profile;

namespace vivaldi {
class StatsReporter;
}

class VivaldiBrowserMainExtraParts : public ChromeBrowserMainExtraParts {
 public:
  VivaldiBrowserMainExtraParts();
  ~VivaldiBrowserMainExtraParts() override;

  // Overridden from ChromeBrowserMainExtraParts:
  void PostEarlyInitialization() override;
  void PreProfileInit() override;

  // We cannot override PostProfileInit() as we need Profile instance. Instead
  // the patched ChromeBrowserMainParts::PreMainMessageLoopRunImpl calls
  // this static method below with the proper Profile*
  // argument.
  static void PostProfileInit(Profile* profile);

  static VivaldiBrowserMainExtraParts* Create();

 private:
  void EnsureBrowserContextKeyedServiceFactoriesBuilt();

  std::unique_ptr<vivaldi::StatsReporter> stats_reporter_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiBrowserMainExtraParts);
};

#endif  // EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_H_
