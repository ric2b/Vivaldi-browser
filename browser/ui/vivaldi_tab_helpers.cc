// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "browser/ui/vivaldi_tab_helpers.h"

#include "app/vivaldi_apptools.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_filter/chrome_content_subresource_filter_web_contents_helper_factory.h"

#include "components/adverse_adblocking/adverse_ad_filter_list.h"
#include "components/adverse_adblocking/adverse_ad_filter_list_factory.h"
#include "components/adverse_adblocking/vivaldi_subresource_filter_throttle_manager.h"

#include "content/public/browser/web_contents.h"

#include "extensions/buildflags/buildflags.h"
#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/api/tabs/tabs_private_api.h"
#endif

using content::WebContents;

namespace vivaldi {
void VivaldiAttachTabHelpers(WebContents* web_contents) {
  if (vivaldi::IsVivaldiRunning() || vivaldi::ForcedVivaldiRunning()) {
    VivaldiSubresourceFilterAdblockingThrottleManager::
        CreateSubresourceFilterWebContentsHelper(web_contents);

    AdverseAdFilterListService* adblock_list =
        VivaldiAdverseAdFilterListFactory::GetForProfile(
            Profile::FromBrowserContext(web_contents->GetBrowserContext()));
    VivaldiSubresourceFilterAdblockingThrottleManager::FromWebContents(
        web_contents)
        ->set_adblock_list(adblock_list);

    CreateSubresourceFilterWebContentsHelper(web_contents);
  }
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (vivaldi::IsVivaldiRunning() || vivaldi::ForcedVivaldiRunning()) {
    extensions::VivaldiPrivateTabObserver::CreateForWebContents(web_contents);
  }
#endif

}
}  // namespace vivaldi
