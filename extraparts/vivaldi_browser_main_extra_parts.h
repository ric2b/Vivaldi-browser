// Copyright (c) 2015-2018 Vivaldi Technologies AS. All rights reserved

#ifndef EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_H_
#define EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_H_

#include <memory>

#include "browser/translate/vivaldi_translate_language_list.h"
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
  VivaldiBrowserMainExtraParts(const VivaldiBrowserMainExtraParts&) = delete;
  VivaldiBrowserMainExtraParts& operator=(const VivaldiBrowserMainExtraParts&) =
      delete;

  // Overridden from ChromeBrowserMainExtraParts:
  void PostEarlyInitialization() override;
  void PreProfileInit() override;

  void PostProfileInit(Profile* profile, bool is_initial_profile) override;

  void PreMainMessageLoopRun() override;
  void PostMainMessageLoopRun() override;

  void PostDestroyThreads() override;

  static std::unique_ptr<VivaldiBrowserMainExtraParts> Create();

 private:
  void EnsureBrowserContextKeyedServiceFactoriesBuilt();

  std::unique_ptr<vivaldi::StatsReporter> stats_reporter_;
  std::unique_ptr<translate::VivaldiTranslateLanguageList>
      translate_language_list_;
};

// Used for non-Vivaldi running
class VivaldiBrowserMainExtraPartsSmall : public ChromeBrowserMainExtraParts {
 public:
  VivaldiBrowserMainExtraPartsSmall() {}
  ~VivaldiBrowserMainExtraPartsSmall() override;
  VivaldiBrowserMainExtraPartsSmall(const VivaldiBrowserMainExtraPartsSmall&) =
      delete;
  VivaldiBrowserMainExtraPartsSmall& operator=(
      const VivaldiBrowserMainExtraPartsSmall&) =
      delete;

  // Overridden from ChromeBrowserMainExtraParts:
  void PostEarlyInitialization() override;
  void PreProfileInit() override;

  void PostProfileInit(Profile* profile, bool is_initial_profile) override;

  void PreMainMessageLoopRun() override;
  void PostMainMessageLoopRun() override;

  void PostDestroyThreads() override;

  static std::unique_ptr<VivaldiBrowserMainExtraPartsSmall> Create();

 private:
  void EnsureBrowserContextKeyedServiceFactoriesBuilt();
};

#endif  // EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_H_
