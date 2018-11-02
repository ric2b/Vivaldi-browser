// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_WIN_H_
#define EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_WIN_H_

#include "extraparts/vivaldi_browser_main_extra_parts.h"

class VivaldiBrowserMainExtraPartsWin : public VivaldiBrowserMainExtraParts {
 public:
  VivaldiBrowserMainExtraPartsWin();
  ~VivaldiBrowserMainExtraPartsWin() override;

  void PostEarlyInitialization() override;

  DISALLOW_COPY_AND_ASSIGN(VivaldiBrowserMainExtraPartsWin);
};

#endif  // EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_WIN_H_
