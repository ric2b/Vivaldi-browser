// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTRAPARTS_VIVALDI_MAIN_DELEGATE_H_
#define EXTRAPARTS_VIVALDI_MAIN_DELEGATE_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/app/android/chrome_main_delegate_android.h"
#else
#include "chrome/app/chrome_main_delegate.h"
#endif

class VivaldiMainDelegate
#if BUILDFLAG(IS_ANDROID)
    : public ChromeMainDelegateAndroid
#else
    : public ChromeMainDelegate
#endif
{
 public:
  VivaldiMainDelegate();
#if !BUILDFLAG(IS_ANDROID)
  explicit VivaldiMainDelegate(const StartupTimestamps& timestamps);
#endif
  ~VivaldiMainDelegate() override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;

  std::optional<int> BasicStartupComplete() override;
};

#if !BUILDFLAG(IS_ANDROID)
class VivaldiTestMainDelegate : public VivaldiMainDelegate {
 public:
  VivaldiTestMainDelegate() : VivaldiMainDelegate() {}
  explicit VivaldiTestMainDelegate(const StartupTimestamps& timestamps)
      : VivaldiMainDelegate(timestamps) {}
#if BUILDFLAG(IS_WIN)
  bool ShouldHandleConsoleControlEvents() override;
#endif
};
#endif

#endif  // EXTRAPARTS_VIVALDI_MAIN_DELEGATE_H_
