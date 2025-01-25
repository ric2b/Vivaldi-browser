// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "ui/vivaldi_document_loader.h"

#include "app/vivaldi_constants.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "components/zoom/zoom_controller.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/mojom/view_type.mojom.h"

#include "chrome/browser/ui/autofill/autofill_client_provider.h"
#include "chrome/browser/ui/autofill/autofill_client_provider_factory.h"
#define VIVALDI_CORE_DOCUMENT "main.html"

VivaldiDocumentLoader::VivaldiDocumentLoader(
    Profile* profile,
    const extensions::Extension* vivaldi_extension)
    : vivaldi_extension_(vivaldi_extension) {

  scoped_refptr<content::SiteInstance> site_instance =
      content::SiteInstance::CreateForURL(profile, vivaldi_extension_->url());

  content::WebContents::CreateParams create_params(profile,
                                                   site_instance.get());
  vivaldi_web_contents_ = content::WebContents::Create(create_params);

  extensions::SetViewType(vivaldi_web_contents_.get(),
                          extensions::mojom::ViewType::kExtensionBackgroundPage);

  vivaldi_web_contents_->SetDelegate(this);

  // Needed for extension functions.
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      vivaldi_web_contents_.get());
  // Need even if not used.
  zoom::ZoomController::CreateForWebContents(vivaldi_web_contents_.get());

  // Needed for chrome.autofillPrivate API
  autofill::AutofillClientProvider& autofill_client_provider =
      autofill::AutofillClientProviderFactory::GetForProfile(profile);
  autofill_client_provider.CreateClientForWebContents(vivaldi_web_contents_.get());

  content::WebContentsObserver::Observe(vivaldi_web_contents_.get());

}

void VivaldiDocumentLoader::Load() {
  GURL resource_url = vivaldi_extension_->GetResourceURL(VIVALDI_CORE_DOCUMENT);
  vivaldi_web_contents_->GetController().LoadURL(
      resource_url, content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
}

VivaldiDocumentLoader::~VivaldiDocumentLoader() {}

void VivaldiDocumentLoader::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  start_time_ = base::TimeTicks::Now();
}

void VivaldiDocumentLoader::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      (!navigation_handle->IsInMainFrame() &&
       !navigation_handle->HasSubframeNavigationEntryCommitted()))
    return;

  end_time_ = base::TimeTicks::Now();
  LOG(INFO) << " VivaldiDocumentLoader::VivaldiDocumentLoader done loading "
             << end_time_ - start_time_;
}

bool VivaldiDocumentLoader::ShouldSuppressDialogs(
    content::WebContents* source) {
  return true;
}

bool VivaldiDocumentLoader::IsNeverComposited(
    content::WebContents* web_contents) {
  return true;
}
