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
#if defined(CHROME_MULTIPLE_DLL_CHILD)
  return NULL;
#else
  if (chrome_content_browser_client_ == nullptr) {
    DCHECK(!startup_data_);
    startup_data_ = std::make_unique<StartupData>();

    chrome_content_browser_client_ =
        std::make_unique<VivaldiContentBrowserClient>(startup_data_.get());
  }
  return chrome_content_browser_client_.get();
#endif
}
