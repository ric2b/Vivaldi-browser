// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTRAPARTS_VIVALDI_MAIN_DELEGATE_H_
#define EXTRAPARTS_VIVALDI_MAIN_DELEGATE_H_

#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "chrome/app/android/chrome_main_delegate_android.h"
#else
#include "chrome/app/chrome_main_delegate.h"
#endif

class VivaldiMainDelegate
#if defined(OS_ANDROID)
    : public ChromeMainDelegateAndroid
#else
    : public ChromeMainDelegate
#endif
{
 public:
  VivaldiMainDelegate();
#if !defined(OS_ANDROID)
  explicit VivaldiMainDelegate(base::TimeTicks exe_entry_point_ticks);
#endif
  ~VivaldiMainDelegate() override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
};

#if !defined(OS_ANDROID)
class VivaldiTestMainDelegate : public VivaldiMainDelegate {
 public:
  VivaldiTestMainDelegate() : VivaldiMainDelegate() {}
  explicit VivaldiTestMainDelegate(base::TimeTicks exe_entry_point_ticks)
      : VivaldiMainDelegate(exe_entry_point_ticks) {}
#if defined(OS_WIN)
  bool ShouldHandleConsoleControlEvents() override;
#endif
};
#endif

#endif  // EXTRAPARTS_VIVALDI_MAIN_DELEGATE_H_
