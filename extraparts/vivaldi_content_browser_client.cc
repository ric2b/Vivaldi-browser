// Copyright (c) 2019-2020 Vivaldi Technologies AS. All rights reserved

#include "extraparts/vivaldi_content_browser_client.h"

#include "build/build_config.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/profiles/profile.h"
#include "components/translate/content/common/translate.mojom.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"

#include "app/vivaldi_apptools.h"
#include "browser/translate/vivaldi_translate_frame_binder.h"
#include "components/adverse_adblocking/adverse_ad_filter_list.h"
#include "components/adverse_adblocking/adverse_ad_filter_list_factory.h"
#include "components/adverse_adblocking/vivaldi_subresource_filter_throttle_manager.h"
#include "components/content_injection/frame_injection_helper.h"
#include "components/content_injection/mojom/content_injection.mojom.h"
#include "components/request_filter/adblock_filter/adblock_cosmetic_filter.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_content.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_factory.h"
#include "components/request_filter/adblock_filter/interstitial/document_blocked_throttle.h"
#include "components/request_filter/adblock_filter/mojom/adblock_cosmetic_filter.mojom.h"
#include "extraparts/vivaldi_browser_main_extra_parts.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#if !BUILDFLAG(IS_ANDROID)
#include "extensions/helper/vivaldi_frame_host_service_impl.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "apps/switches.h"
#include "base/vivaldi_switches.h"
#include "sandbox/policy/switches.h"
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
  } else {
    main_parts_actual->AddParts(VivaldiBrowserMainExtraPartsSmall::Create());
  }
  return main_parts;
}

#if !BUILDFLAG(IS_ANDROID)
void
VivaldiContentBrowserClient::CreateThrottlesForNavigation(
    content::NavigationThrottleRegistry& registry) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::NavigationHandle *handle = &registry.GetNavigationHandle();

  ChromeContentBrowserClient::CreateThrottlesForNavigation(registry);

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
          handle, registry);
    }
  }
  registry.AddThrottle(
      std::make_unique<adblock_filter::DocumentBlockedThrottle>(handle));
}

bool VivaldiContentBrowserClient::CanCommitURL(
    content::RenderProcessHost* process_host,
    const GURL& url) {
  if (vivaldi::IsVivaldiRunning())
    return true;

  return ChromeContentBrowserClient::CanCommitURL(process_host, url);
}

#endif  // !IS_ANDROID

#ifdef VIVALDI_V8_CONTEXT_SNAPSHOT
#if BUILDFLAG(IS_LINUX)
void VivaldiContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  if (!browser_command_line.HasSwitch(apps::kLoadAndLaunchApp)) {
    std::string process_type = command_line->GetSwitchValueASCII(
        sandbox::policy::switches::kProcessType);
    if (process_type == sandbox::policy::switches::kZygoteProcessType) {
      bool is_gpu_zygote = command_line->HasSwitch(
          sandbox::policy::switches::kNoZygoteSandbox);
      if (!is_gpu_zygote) {
        command_line->AppendSwitch(switches::kVivaldiSnapshotProcess);
      }
    }
  }
  return ChromeContentBrowserClient::AppendExtraCommandLineSwitches(
      command_line,
      child_process_id);
}
#endif  // VIVALDI_V8_CONTEXT_SNAPSHOT
#endif  // IS_LINUX

void BindCosmeticFilter(
    content::RenderFrameHost* frame,
    mojo::PendingReceiver<adblock_filter::mojom::CosmeticFilter> receiver) {
  mojo::MakeSelfOwnedReceiver(
      adblock_filter::RuleServiceFactory::GetForBrowserContext(
          frame->GetProcess()->GetBrowserContext())
          ->MakeCosmeticFilter(frame),
      std::move(receiver));
}

void VivaldiContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  map->Add<adblock_filter::mojom::CosmeticFilter>(
      base::BindRepeating(BindCosmeticFilter));
  map->Add<content_injection::mojom::FrameInjectionHelper>(
      base::BindRepeating(&content_injection::FrameInjectionHelper::Create));

  ChromeContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
      render_frame_host, map);

  // Register these bindings after Chromium bindings, so we can
  // replace them with our own, if needed.

  map->Add<translate::mojom::ContentTranslateDriver>(
      base::BindRepeating(&vivaldi::BindVivaldiContentTranslateDriver));
}
