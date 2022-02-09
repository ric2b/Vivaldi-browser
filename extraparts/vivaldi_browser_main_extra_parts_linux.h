// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_LINUX_H_
#define EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_LINUX_H_

#include "extraparts/vivaldi_browser_main_extra_parts.h"

class VivaldiBrowserMainExtraPartsLinux : public VivaldiBrowserMainExtraParts {
 public:
  VivaldiBrowserMainExtraPartsLinux();
  ~VivaldiBrowserMainExtraPartsLinux() override;
  VivaldiBrowserMainExtraPartsLinux(const VivaldiBrowserMainExtraPartsLinux&) =
      delete;
  VivaldiBrowserMainExtraPartsLinux& operator=(
      const VivaldiBrowserMainExtraPartsLinux&) = delete;
};

#endif  // EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_LINUX_H_
