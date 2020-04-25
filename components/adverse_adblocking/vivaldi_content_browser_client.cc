// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/adverse_adblocking/vivaldi_content_browser_client.h"

#include "app/vivaldi_apptools.h"
#include "components/adverse_adblocking/adverse_ad_filter_list.h"
#include "components/adverse_adblocking/adverse_ad_filter_list_factory.h"
#include "components/adverse_adblocking/vivaldi_subresource_filter_client.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"

VivaldiContentBrowserClient::VivaldiContentBrowserClient(
    StartupData* startup_data)
    : ChromeContentBrowserClient(startup_data) {}

std::vector<std::unique_ptr<content::NavigationThrottle>>
VivaldiContentBrowserClient::CreateThrottlesForNavigation(
    content::NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto throttles =
      ChromeContentBrowserClient::CreateThrottlesForNavigation(handle);

  AdverseAdFilterListService* adblock_list =
      VivaldiAdverseAdFilterListFactory::GetForProfile(
          Profile::FromBrowserContext(
              handle->GetStartingSiteInstance()->GetBrowserContext()));

  if ((vivaldi::IsVivaldiRunning() || vivaldi::ForcedVivaldiRunning()) &&
      adblock_list->has_sites()) {
    content::WebContents* web_contents = handle->GetWebContents();
    if (auto* subresource_filter_client =
            VivaldiSubresourceFilterClient::FromWebContents(web_contents)) {
      subresource_filter_client->SetAdblockList(adblock_list);
      subresource_filter_client->MaybeAppendNavigationThrottles(handle,
                                                                &throttles);
    }
  }

  return throttles;
}

VivaldiContentBrowserClient::~VivaldiContentBrowserClient() {}
