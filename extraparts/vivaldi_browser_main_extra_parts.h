// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef VIVALDI_BORWSER_MAIN_EXTRA_PARTS_H_
#define VIVALDI_BORWSER_MAIN_EXTRA_PARTS_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"

class VivaldiBrowserMainExtraParts : public ChromeBrowserMainExtraParts {
 public:
  VivaldiBrowserMainExtraParts();
  ~VivaldiBrowserMainExtraParts() override;

  // Overridden from ChromeBrowserMainExtraParts:
  void PostEarlyInitialization() override;

#if defined(OS_MACOSX)
  void PostEarlyInitializationMac();
  void PreProfileInitMac();
#endif
#if defined(OS_WIN)
  void PostEarlyInitializationWin();
#endif
#if defined(OS_LINUX)
  void PostEarlyInitializationLinux();
#endif

  void PreProfileInit() override;
  void PostProfileInit() override;

 private:
  void EnsureBrowserContextKeyedServiceFactoriesBuilt();

  DISALLOW_COPY_AND_ASSIGN(VivaldiBrowserMainExtraParts);
};

#endif // VIVALDI_BORWSER_MAIN_EXTRA_PARTS_H_
