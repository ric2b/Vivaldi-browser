// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "ui/vivaldi_rootdocument_handler.h"

#include <set>

#include "base/containers/contains.h"
#include "chrome/browser/browser_process.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/extension_registry.h"

namespace extensions {

// Paths in this set will not create a VivaldiRootDocumentHandler.
using GatherProfileSet = std::set<base::FilePath>;
GatherProfileSet& ProfilesWithNoVivaldi() {
  static base::NoDestructor<GatherProfileSet> profilepaths_to_avoid;
  return *profilepaths_to_avoid;
}

// static
VivaldiRootDocumentHandler*
VivaldiRootDocumentHandlerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  if (ProfileShouldNotUseVivaldiClient(
          Profile::FromBrowserContext(browser_context)->GetPath())) {
    return nullptr;
  }
  return static_cast<VivaldiRootDocumentHandler*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
VivaldiRootDocumentHandlerFactory*
VivaldiRootDocumentHandlerFactory::GetInstance() {
  static base::NoDestructor<VivaldiRootDocumentHandlerFactory> instance;
  return instance.get();
}

VivaldiRootDocumentHandlerFactory::VivaldiRootDocumentHandlerFactory()
    : BrowserContextKeyedServiceFactory(
          "VivaldiRootDocumentHandler",
          BrowserContextDependencyManager::GetInstance()) {

  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
}

VivaldiRootDocumentHandlerFactory::~VivaldiRootDocumentHandlerFactory() {}

KeyedService* VivaldiRootDocumentHandlerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new VivaldiRootDocumentHandler(context);
}

bool VivaldiRootDocumentHandlerFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

bool VivaldiRootDocumentHandlerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

content::BrowserContext*
VivaldiRootDocumentHandlerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Redirected in incognito.
  return ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
      context);

}

VivaldiRootDocumentHandler::VivaldiRootDocumentHandler(
    content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {

  infobar_container_ = std::make_unique<::vivaldi::InfoBarContainerWebProxy>(
      this);

  observed_profiles_.AddObservation(profile_);
  if (profile_->HasPrimaryOTRProfile())
    observed_profiles_.AddObservation(
        profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  ExtensionRegistry::Get(profile_)->AddObserver(this);
}

VivaldiRootDocumentHandler::~VivaldiRootDocumentHandler() {
  DCHECK(!vivaldi_document_loader_);
  DCHECK(!vivaldi_document_loader_off_the_record_);
}

void VivaldiRootDocumentHandler::OnOffTheRecordProfileCreated(
    Profile* off_the_record) {
  observed_profiles_.AddObservation(off_the_record);

  DCHECK(vivaldi_extension_);
  vivaldi_document_loader_off_the_record_ =
      new VivaldiDocumentLoader(off_the_record, vivaldi_extension_);

  otr_document_observer_ = std::make_unique<DocumentContentsObserver>(
      this, vivaldi_document_loader_off_the_record_->GetWebContents());

  vivaldi_document_loader_off_the_record_->Load();
}

void VivaldiRootDocumentHandler::OnProfileWillBeDestroyed(Profile* profile) {
  observed_profiles_.RemoveObservation(profile);
  if (profile->IsOffTheRecord() && profile->GetOriginalProfile() == profile_) {
    vivaldi_document_loader_off_the_record_.ClearAndDelete();
  } else if (profile == profile_) {
    // this will be destroyed by KeyedServiceFactory::ContextShutdown
  }
}

void VivaldiRootDocumentHandler::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (extension->id() == ::vivaldi::kVivaldiAppId &&
      !vivaldi_document_loader_) {
    vivaldi_document_loader_ = new VivaldiDocumentLoader(
        Profile::FromBrowserContext(browser_context), extension);
    vivaldi_extension_ = extension;
    document_observer_ = std::make_unique<DocumentContentsObserver>(
        this, vivaldi_document_loader_->GetWebContents());
    vivaldi_document_loader_->Load();
  }
}

VivaldiRootDocumentHandler::DocumentContentsObserver::DocumentContentsObserver(
    VivaldiRootDocumentHandler* handler,
    content::WebContents* contents)
    : WebContentsObserver(contents), root_doc_handler_(handler) {}

void VivaldiRootDocumentHandler::DocumentContentsObserver::DOMContentLoaded(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host->GetParent()) {
    // Nothing to do for sub frames here.
    return;
  }
  auto* web_contents = content::WebContents::FromRenderFrameHost(render_frame_host);

  if (web_contents == root_doc_handler_->GetWebContents()) {
    root_doc_handler_->document_loader_is_ready_ = true;
  } else if (web_contents == root_doc_handler_->GetOTRWebContents()) {
    root_doc_handler_->otr_document_loader_is_ready_ = true;
  }
  root_doc_handler_->InformObservers();
}

void VivaldiRootDocumentHandler::InformObservers() {
  for (auto& observer : observers_) {
    observer.OnRootDocumentDidFinishNavigation();
  }
}

content::WebContents* VivaldiRootDocumentHandler::GetWebContents() {
  return vivaldi_document_loader_ ? vivaldi_document_loader_->GetWebContents()
                                  : nullptr;
}

content::WebContents* VivaldiRootDocumentHandler::GetOTRWebContents() {
  return vivaldi_document_loader_off_the_record_
             ? vivaldi_document_loader_off_the_record_->GetWebContents()
             : nullptr;
}

void VivaldiRootDocumentHandler::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  if (extension->id() == ::vivaldi::kVivaldiAppId) {
    // not much we can do if vivaldi goes away
    vivaldi_extension_ = nullptr;
  }
}

void VivaldiRootDocumentHandler::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  if (extension->id() == ::vivaldi::kVivaldiAppId) {
    // not much we can do if vivaldi goes away
  }
}

void VivaldiRootDocumentHandler::Shutdown() {
  ExtensionRegistry::Get(profile_)->RemoveObserver(this);
  vivaldi_document_loader_.ClearAndDelete();
  vivaldi_document_loader_off_the_record_.ClearAndDelete();
}

void VivaldiRootDocumentHandler::AddObserver(
    VivaldiRootDocumentHandlerObserver* observer) {
  observers_.AddObserver(observer);

  // Check if we already loaded the portal-document for the profile and report.
  content::BrowserContext* observercontext =
      observer->GetRootDocumentWebContents()->GetBrowserContext();
  if ((document_loader_is_ready_ &&
       observercontext ==
           vivaldi_document_loader_->GetWebContents()->GetBrowserContext()) ||
      (otr_document_loader_is_ready_ &&
       observercontext ==
           vivaldi_document_loader_off_the_record_->GetWebContents()
               ->GetBrowserContext())) {
    observer->OnRootDocumentDidFinishNavigation();
  }
}

void VivaldiRootDocumentHandler::RemoveObserver(
    VivaldiRootDocumentHandlerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MarkProfilePathForNoVivaldiClient(const base::FilePath& path) {
  // No need to mark the path more than once.
  DCHECK(!base::Contains(ProfilesWithNoVivaldi(), path));
  ProfilesWithNoVivaldi().insert(path);
}

void ClearProfilePathForNoVivaldiClient(const base::FilePath& path) {
  // This might need syncronization.
  ProfilesWithNoVivaldi().erase(path);
}

bool ProfileShouldNotUseVivaldiClient(const base::FilePath& path) {
  return base::Contains(ProfilesWithNoVivaldi(), path);
}

}  // namespace extensions
