// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "extraparts/vivaldi_content_browser_client.h"

#include "app/vivaldi_apptools.h"
#include "chrome/browser/chrome_browser_main.h"
#include "components/adverse_adblocking/adverse_ad_filter_list.h"
#include "components/adverse_adblocking/adverse_ad_filter_list_factory.h"
#include "components/adverse_adblocking/vivaldi_subresource_filter_client.h"

#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"

#include "extraparts/media_renderer_host_message_filter.h"
#include "extraparts/vivaldi_browser_main_extra_parts.h"

VivaldiContentBrowserClient::VivaldiContentBrowserClient(
    StartupData* startup_data)
    : ChromeContentBrowserClient(startup_data) {}

VivaldiContentBrowserClient::~VivaldiContentBrowserClient() {}

std::unique_ptr<content::BrowserMainParts>
VivaldiContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  std::unique_ptr<content::BrowserMainParts> main_parts =
      ChromeContentBrowserClient::CreateBrowserMainParts(parameters);

  ChromeBrowserMainParts* main_parts_actual =
      static_cast<ChromeBrowserMainParts*>(main_parts.get());

  main_parts_actual->AddParts(VivaldiBrowserMainExtraParts::Create());

  return main_parts;
}

#if !defined(OS_ANDROID)
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

bool VivaldiContentBrowserClient::CanCommitURL(
  content::RenderProcessHost* process_host,
  const GURL& url) {
  if (vivaldi::IsVivaldiRunning())
    return true;

  return ChromeContentBrowserClient::CanCommitURL(process_host, url);
}

void VivaldiContentBrowserClient::RenderProcessWillLaunch(
    content::RenderProcessHost* host) {
  ChromeContentBrowserClient::RenderProcessWillLaunch(host);
  int id = host->GetID();
  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());
  host->AddFilter(new vivaldi::MediaRendererHostMessageFilter(id, profile));
}

#endif  // !OS_ANDROID

std::string VivaldiContentBrowserClient::GetStoragePartitionIdForSite(
    content::BrowserContext* browser_context,
    const GURL& site) {
  if (site.SchemeIs("chrome-extension")) {
    std::string partition_id = site.spec();

    DCHECK(IsValidStoragePartitionId(browser_context, partition_id));
    return partition_id;
  }

  return ChromeContentBrowserClient::GetStoragePartitionIdForSite(
      browser_context, site);
}
