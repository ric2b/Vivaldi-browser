// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#ifndef EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_ANDROID_H_
#define EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_ANDROID_H_

#include "extraparts/vivaldi_browser_main_extra_parts.h"

class VivaldiBrowserMainExtraPartsAndroid
    : public VivaldiBrowserMainExtraParts {
 public:
  VivaldiBrowserMainExtraPartsAndroid();
  ~VivaldiBrowserMainExtraPartsAndroid() override;
  VivaldiBrowserMainExtraPartsAndroid(
      const VivaldiBrowserMainExtraPartsAndroid&) = delete;
  VivaldiBrowserMainExtraPartsAndroid& operator=(
      const VivaldiBrowserMainExtraPartsAndroid&) = delete;

  // VivaldiBrowserMainExtraParts override.
  void PostProfileInit(Profile* profile, bool is_initial_profile) override;
  //void PreProfileInit() override;

 //private:
  //void EnsureBrowserContextKeyedServiceFactoriesBuilt();
};

#endif  // EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_ANDROID_H_
