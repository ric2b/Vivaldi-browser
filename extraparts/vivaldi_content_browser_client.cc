// Copyright (c) 2019-2020 Vivaldi Technologies AS. All rights reserved

#include "extraparts/vivaldi_content_browser_client.h"

#include "build/build_config.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/profiles/profile.h"
#include "components/translate/content/common/translate.mojom.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"

#include "app/vivaldi_apptools.h"
#include "browser/translate/vivaldi_translate_frame_binder.h"
#include "components/adverse_adblocking/adverse_ad_filter_list.h"
#include "components/adverse_adblocking/adverse_ad_filter_list_factory.h"
#include "components/adverse_adblocking/vivaldi_subresource_filter_throttle_manager.h"
#include "components/content_injection/frame_injection_helper.h"
#include "components/content_injection/mojom/content_injection.mojom.h"
#include "components/request_filter/adblock_filter/adblock_cosmetic_filter.h"
#include "components/request_filter/adblock_filter/interstitial/document_blocked_throttle.h"
#include "components/request_filter/adblock_filter/mojom/adblock_cosmetic_filter.mojom.h"
#include "extraparts/vivaldi_browser_main_extra_parts.h"

#if !BUILDFLAG(IS_ANDROID)
#include "extensions/helper/vivaldi_frame_host_service_impl.h"
#endif

VivaldiContentBrowserClient::VivaldiContentBrowserClient()
    : ChromeContentBrowserClient() {}

VivaldiContentBrowserClient::~VivaldiContentBrowserClient() {}

std::unique_ptr<content::BrowserMainParts>
VivaldiContentBrowserClient::CreateBrowserMainParts(bool is_integration_test) {
  std::unique_ptr<content::BrowserMainParts> main_parts =
      ChromeContentBrowserClient::CreateBrowserMainParts(is_integration_test);

  ChromeBrowserMainParts* main_parts_actual =
      static_cast<ChromeBrowserMainParts*>(main_parts.get());

  if (vivaldi::IsVivaldiRunning() || vivaldi::ForcedVivaldiRunning()) {
    main_parts_actual->AddParts(VivaldiBrowserMainExtraParts::Create());
  }
  else {
    main_parts_actual->AddParts(VivaldiBrowserMainExtraPartsSmall::Create());
  }
  return main_parts;
}

#if !BUILDFLAG(IS_ANDROID)
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
    if (auto* vivaldi_subresource_throttle_manager =
            VivaldiSubresourceFilterAdblockingThrottleManager::FromWebContents(
                web_contents)) {
      vivaldi_subresource_throttle_manager->MaybeAppendNavigationThrottles(
          handle, &throttles);
    }
  }

  throttles.push_back(
      std::make_unique<adblock_filter::DocumentBlockedThrottle>(handle));

  return throttles;
}

bool VivaldiContentBrowserClient::CanCommitURL(
    content::RenderProcessHost* process_host,
    const GURL& url) {
  if (vivaldi::IsVivaldiRunning())
    return true;

  return ChromeContentBrowserClient::CanCommitURL(process_host, url);
}

#endif  // !IS_ANDROID

void VivaldiContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  ChromeContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
      render_frame_host, map);

  // Register Vivaldi bindings after Chromium bindings, so we can
  // replace them with our own, if needed.
  //map->Add<adblock_filter::mojom::CosmeticFilter>(
  //   base::BindRepeating(&adblock_filter::CosmeticFilter::Create));
  //map->Add<content_injection::mojom::FrameInjectionHelper>(
  //    base::BindRepeating(&content_injection::FrameInjectionHelper::Create));

  map->Add<translate::mojom::ContentTranslateDriver>(
      base::BindRepeating(&vivaldi::BindVivaldiContentTranslateDriver));

#if !BUILDFLAG(IS_ANDROID)
  //map->Add<vivaldi::mojom::VivaldiFrameHostService>(
  //    base::BindRepeating(&VivaldiFrameHostServiceImpl::BindHandler));
#endif
}
