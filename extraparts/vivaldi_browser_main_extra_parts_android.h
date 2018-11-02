// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_ANDROID_H_
#define EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_ANDROID_H_

#include "extraparts/vivaldi_browser_main_extra_parts.h"

class VivaldiBrowserMainExtraPartsAndroid : public VivaldiBrowserMainExtraParts {
 public:
  VivaldiBrowserMainExtraPartsAndroid();
  ~VivaldiBrowserMainExtraPartsAndroid() override;

  DISALLOW_COPY_AND_ASSIGN(VivaldiBrowserMainExtraPartsAndroid);
};

#endif  // EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_ANDROID_H_
