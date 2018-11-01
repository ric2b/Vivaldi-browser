// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTRAPARTS_VIVALDI_MAIN_DELEGATE_H_
#define EXTRAPARTS_VIVALDI_MAIN_DELEGATE_H_

#include "chrome/app/chrome_main_delegate.h"

class VivaldiMainDelegate : public ChromeMainDelegate {
 public:
  VivaldiMainDelegate();
  explicit VivaldiMainDelegate(base::TimeTicks exe_entry_point_ticks);
  ~VivaldiMainDelegate() override;
};

#endif  // EXTRAPARTS_VIVALDI_MAIN_DELEGATE_H_
