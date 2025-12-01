// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "components/drm_helper/vivaldi_drm_tab_helper.h"
#include "chrome/browser/browser_process.h"
#include "extensions/api/auto_update/auto_update_api.h"
#include "components/update_client/crx_update_item.h"

#if !BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "ui/vivaldi_infobar_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "app/vivaldi_resources.h"
#endif

namespace drm_helper {

#if !BUILDFLAG(IS_LINUX)
static constexpr char kWidevineComponentID[] =
    "oimompecagnajdejgnnjijobebaeigek";
#endif

void DRMContentTabHelper::Create(
    content::RenderFrameHost* frame_host,
    mojo::PendingAssociatedReceiver<vivaldi::mojom::VivaldiEncryptedMediaAccess>
        receiver) {
  auto* contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!contents)
    return;

  auto* instance = DRMContentTabHelper::FromWebContents(contents);
  if (!instance)
    return;

  instance->media_access_receivers_.Bind(frame_host, std::move(receiver));
}

DRMContentTabHelper::~DRMContentTabHelper() {}

DRMContentTabHelper::DRMContentTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<DRMContentTabHelper>(*web_contents),
      media_access_receivers_{web_contents, this} {
#if !BUILDFLAG(IS_LINUX)
  auto* auto_update = extensions::AutoUpdateAPI::GetFactoryInstance()->Get(
      web_contents->GetBrowserContext());

  // In private windows, auto_update is not available.
  if (!auto_update)
    return;

  // Don't install observer if the module already was available.
  if (auto_update->WasWidevineAvailable())
    return;

  // We are going to observe the module installation process...
  auto* component_updater = g_browser_process->component_updater();
  observer_.Observe(component_updater);
#endif
}

void DRMContentTabHelper::OnEvent(const update_client::CrxUpdateItem& item) {
#if !BUILDFLAG(IS_LINUX)
  // For non-linux platforms, we want to handle tab reloading here (CDM gets to
  // be used).
  if (item.id == kWidevineComponentID) {
    if (item.state ==
        update_client::ComponentState::kUpdated) {
      LOG(INFO) << "DRMContentTabHelper: Informing widevine was updated.";
      HandleModuleUpdated();
      return;
    }
  }
#endif
}

#if !BUILDFLAG(IS_LINUX)
void DRMContentTabHelper::HandleModuleUpdated() {
  if (was_requested_) {
    infobars::ContentInfoBarManager* infobar_manager =
        infobars::ContentInfoBarManager::FromWebContents(web_contents());
    if (infobar_manager) {
      VivaldiInfoBarDelegate::SpawnParams spawn_params(
          l10n_util::GetStringUTF16(IDS_VIVALDI_RELOAD_FOR_ENCRYPTED_CONTENT),
          base::BindOnce(
              [](content::WebContents* web_contents) {
                DCHECK(web_contents);
                web_contents->GetController().Reload(
                    content::ReloadType::NORMAL, true);
              },
              web_contents()));

      spawn_params.buttons = VivaldiInfoBarDelegate::BUTTON_OK;
      spawn_params.button_labels[VivaldiInfoBarDelegate::BUTTON_OK] =
          l10n_util::GetStringUTF16(IDS_VIVALDI_RELOAD);

      VivaldiInfoBarDelegate::CreateForVivaldi(infobar_manager, std::move(spawn_params));
    }
  }

  // We can de-register the observer now.
  observer_.Reset();
}
#endif

void DRMContentTabHelper::NotifyEncryptedMediaAccess(
    const std::string& key_system) {
  // So far we're only interested in widevine.
  if (key_system != "com.widevine.alpha")
    return;

  auto* auto_update = extensions::AutoUpdateAPI::GetFactoryInstance()->Get(
      web_contents()->GetBrowserContext());

  // In private windows, auto_update might not be available.
  if (!auto_update)
    return;

  // Is widevine available? Maybe we need to restart the browser here?
  if (auto_update->WasWidevineAvailable())
    return;

#if !BUILDFLAG(IS_LINUX)
  // This will be used for tab reload later.
  was_requested_ = true;
#endif

  LOG(INFO) << "Encrypted media access was requested, but not yet installed.";
  auto_update->HandleWidevineRequested();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DRMContentTabHelper);

}  // namespace drm_helper
