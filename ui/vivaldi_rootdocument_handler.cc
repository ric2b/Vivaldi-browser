// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "ui/vivaldi_rootdocument_handler.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/extension_registry.h"

namespace extensions {

// static
VivaldiRootDocumentHandler*
VivaldiRootDocumentHandlerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
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
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
}

VivaldiRootDocumentHandler::VivaldiRootDocumentHandler(
    content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  profile_->AddObserver(this);
  ExtensionRegistry::Get(profile_)->AddObserver(this);
}

VivaldiRootDocumentHandler::~VivaldiRootDocumentHandler() {
  DCHECK(!vivaldi_document_loader_);
  DCHECK(!vivaldi_document_loader_off_the_record_);
  profile_->RemoveObserver(this);
}

void VivaldiRootDocumentHandler::OnOffTheRecordProfileCreated(
    Profile* off_the_record) {
  off_the_record->AddObserver(this);
  DCHECK(vivaldi_extension_);
  vivaldi_document_loader_off_the_record_ =
      new VivaldiDocumentLoader(off_the_record, vivaldi_extension_);

  otr_document_observer_ = std::make_unique<DocumentContentsObserver>(
      this, vivaldi_document_loader_off_the_record_->GetWebContents());

  vivaldi_document_loader_off_the_record_->Load();
}

void VivaldiRootDocumentHandler::OnProfileWillBeDestroyed(Profile* profile) {
  if (profile->IsOffTheRecord() && profile->GetOriginalProfile() == profile_) {
    profile->RemoveObserver(this);
    delete vivaldi_document_loader_off_the_record_;
    vivaldi_document_loader_off_the_record_ = nullptr;
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
  delete vivaldi_document_loader_;
  vivaldi_document_loader_ = nullptr;
  delete vivaldi_document_loader_off_the_record_;
  vivaldi_document_loader_off_the_record_ = nullptr;
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

}  // namespace extensions
