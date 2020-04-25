// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extraparts/vivaldi_main_delegate.h"

#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/vivaldi_extensions_client.h"
#endif

#include "components/adverse_adblocking/vivaldi_content_browser_client.h"

VivaldiMainDelegate::VivaldiMainDelegate()
    : VivaldiMainDelegate(base::TimeTicks()) {}

VivaldiMainDelegate::VivaldiMainDelegate(base::TimeTicks exe_entry_point_ticks)
    : ChromeMainDelegate(exe_entry_point_ticks) {}

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
