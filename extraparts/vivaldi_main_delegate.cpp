// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extraparts/vivaldi_main_delegate.h"

#include "extraparts/vivaldi_content_browser_client.h"

VivaldiMainDelegate::VivaldiMainDelegate()
#if !defined(OS_ANDROID)
    : VivaldiMainDelegate(base::TimeTicks())
#endif
{
}

#if !defined(OS_ANDROID)
VivaldiMainDelegate::VivaldiMainDelegate(base::TimeTicks exe_entry_point_ticks)
    : ChromeMainDelegate(exe_entry_point_ticks) {}
#endif

VivaldiMainDelegate::~VivaldiMainDelegate() {}

content::ContentBrowserClient*
VivaldiMainDelegate::CreateContentBrowserClient() {
  if (chrome_content_browser_client_ == nullptr) {
    chrome_content_browser_client_ =
        std::make_unique<VivaldiContentBrowserClient>();
  }
  return chrome_content_browser_client_.get();
}
