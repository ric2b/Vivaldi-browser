// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "ui/vivaldi_rootdocument_handler.h"

#include <set>

#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_extension_system_factory.h"
#include "chrome/browser/extensions/external_install_error_desktop.h"
#include "chrome/browser/extensions/extension_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/schema/browser_action_utilities.h"
#include "extensions/tools/vivaldi_tools.h"

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
  DependsOn(extensions::ChromeExtensionSystemFactory::GetInstance());
}

VivaldiRootDocumentHandlerFactory::~VivaldiRootDocumentHandlerFactory() {}

std::unique_ptr<KeyedService>
VivaldiRootDocumentHandlerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<VivaldiRootDocumentHandler>(context);
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
  CommandService::Get(profile_)->AddObserver(this);
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
    vivaldi_extension_ = extension;
    Profile* profile = Profile::FromBrowserContext(browser_context);
    if (!profile->IsGuestSession()) {
      // NOTE(andre@vivaldi.com) : Guest profile starts as a regular profile,
      // but switches to off the record in
      // ProfileManager::OnProfileCreationFinished. We get to
      //  OnOffTheRecordProfileCreated and proceed there.
      vivaldi_document_loader_ = new VivaldiDocumentLoader(
          Profile::FromBrowserContext(browser_context), extension);
      document_observer_ = std::make_unique<DocumentContentsObserver>(
          this, vivaldi_document_loader_->GetWebContents());
      vivaldi_document_loader_->Load();
    }
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

  vivaldi::extension_action_utils::ExtensionInstallError jserror;
  jserror.id = extension->id();
  VivaldiExtensionDisabledGlobalError::SendGlobalErrorRemoved(browser_context,
                                                              &jserror);
}

void VivaldiRootDocumentHandler::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    bool is_update) {
    // Remove any visible install-errors.
  vivaldi::extension_action_utils::ExtensionInstallError jserror;
  jserror.id = extension->id();
  VivaldiExtensionDisabledGlobalError::SendGlobalErrorRemoved(browser_context,
                                                              &jserror);
}

void VivaldiRootDocumentHandler::Shutdown() {
  ExtensionRegistry::Get(profile_)->RemoveObserver(this);
  CommandService::Get(profile_)->RemoveObserver(this);
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


void VivaldiRootDocumentHandler::AddGlobalError(
    std::unique_ptr<VivaldiExtensionDisabledGlobalError> error) {
  errors_.push_back(std::move(error));
}

void VivaldiRootDocumentHandler::RemoveGlobalError(
    VivaldiExtensionDisabledGlobalError* error) {
  for (auto& err : errors_) {
    if (error == err.get()) {
      errors_.erase(std::ranges::find(errors_, err));
      return;
    }
  }
}

raw_ptr<VivaldiExtensionDisabledGlobalError>
VivaldiRootDocumentHandler::GetGlobalErrorByMenuItemCommandID(int commandid) {
  for (auto& error : errors_) {
    if (commandid == error->MenuItemCommandID()) {
      return error.get();
    }
  }
  return nullptr;
}

void VivaldiRootDocumentHandler::OnExtensionCommandAdded(
    const std::string& extension_id,
    const Command& added_command) {
  // NOTE(daniel@vivaldi.com): Our JS code only has to handle actions, i.e.
  // "Activate the extension" as it's called in vivaldi://extensions. Other
  // extension commands get set in vivaldi_browser_window.cc.
  if (!Command::IsActionRelatedCommand(added_command.command_name()))
    return;
  std::string shortcut_text =
      ::vivaldi::ShortcutText(added_command.accelerator().key_code(),
                              added_command.accelerator().modifiers(), 0);
  ::vivaldi::BroadcastEvent(
      vivaldi::extension_action_utils::OnCommandAdded::kEventName,
      vivaldi::extension_action_utils::OnCommandAdded::Create(extension_id,
                                                              shortcut_text),
      profile_);
}

void VivaldiRootDocumentHandler::OnExtensionCommandRemoved(
    const std::string& extension_id,
    const Command& removed_command) {
  if (!Command::IsActionRelatedCommand(removed_command.command_name()))
    return;
  std::string shortcut_text =
      ::vivaldi::ShortcutText(removed_command.accelerator().key_code(),
                              removed_command.accelerator().modifiers(), 0);

  ::vivaldi::BroadcastEvent(
      vivaldi::extension_action_utils::OnCommandRemoved::kEventName,
      vivaldi::extension_action_utils::OnCommandRemoved::Create(extension_id,
                                                                shortcut_text),
      profile_);
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

/// VivaldiExtensionDisabledGlobalError

VivaldiExtensionDisabledGlobalError::VivaldiExtensionDisabledGlobalError(
    content::BrowserContext* context,
    base::WeakPtr<ExternalInstallError> error)
    : browser_context_(context) {
  external_install_error_ = std::move(error);

  registry_observation_.Observe(ExtensionRegistry::Get(browser_context_));

  VivaldiRootDocumentHandler* root_doc_handler =
      extensions::VivaldiRootDocumentHandlerFactory::GetForBrowserContext(
          browser_context_);

  int unique_id = root_doc_handler->GetExtensionToIdProvider().AddOrGetId(
      external_install_error_->extension_id());

  command_id_ = unique_id;
  extension_id_ = external_install_error_->GetExtension()->id();
  extension_name_ = external_install_error_->GetExtension()->name();

  vivaldi::extension_action_utils::ExtensionInstallError jserror;
  jserror.id = extension_id_;
  jserror.command_id = unique_id;
  jserror.name = extension_name_;
  jserror.error_type =
      vivaldi::extension_action_utils::GlobalErrorType::kInstalled;
  SendGlobalErrorAdded(&jserror);
}

void VivaldiExtensionDisabledGlobalError::SendGlobalErrorAdded(

    vivaldi::extension_action_utils::ExtensionInstallError* jserror) {
  ::vivaldi::BroadcastEvent(
      vivaldi::extension_action_utils::OnExtensionDisabledInstallErrorAdded::
          kEventName,
      vivaldi::extension_action_utils::OnExtensionDisabledInstallErrorAdded::
          Create(*jserror),
      browser_context_);
}

/*static*/
void VivaldiExtensionDisabledGlobalError::SendGlobalErrorRemoved(
    content::BrowserContext* browser_context,
    vivaldi::extension_action_utils::ExtensionInstallError* jserror) {
  ::vivaldi::BroadcastEvent(
      vivaldi::extension_action_utils::OnExtensionDisabledInstallErrorRemoved::
          kEventName,
      vivaldi::extension_action_utils::OnExtensionDisabledInstallErrorRemoved::
          Create(*jserror),
      browser_context);
}

void VivaldiExtensionDisabledGlobalError::SendGlobalErrorRemoved(
    vivaldi::extension_action_utils::ExtensionInstallError* jserror) {
  ::vivaldi::BroadcastEvent(
      vivaldi::extension_action_utils::OnExtensionDisabledInstallErrorRemoved::
          kEventName,
      vivaldi::extension_action_utils::OnExtensionDisabledInstallErrorRemoved::
          Create(*jserror),
      browser_context_);
}

VivaldiExtensionDisabledGlobalError::VivaldiExtensionDisabledGlobalError(
    ExtensionService* service,
    const Extension* extension,
    base::WeakPtr<GlobalErrorWithStandardBubble> disabled_upgrade_error)
    : browser_context_(service->profile()),
      service_(service),
      extension_(extension) {
  disabled_upgrade_error_ = std::move(disabled_upgrade_error);
  extension_id_ = extension_->id();
  extension_name_ = extension_->name();

  registry_observation_.Observe(ExtensionRegistry::Get(service->profile()));

  vivaldi::extension_action_utils::ExtensionInstallError error;

  error.id = extension_id_;
  error.name = extension_name_;

  VivaldiRootDocumentHandler* root_doc_handler =
      extensions::VivaldiRootDocumentHandlerFactory::GetForBrowserContext(
          browser_context_);

  command_id_ =
      root_doc_handler->GetExtensionToIdProvider().AddOrGetId(extension->id());

  error.command_id = command_id_;
  error.error_type = vivaldi::extension_action_utils::GlobalErrorType::kUpgrade;

  SendGlobalErrorAdded(&error);
}

VivaldiExtensionDisabledGlobalError::~VivaldiExtensionDisabledGlobalError() {}

 GlobalErrorWithStandardBubble::Severity
 VivaldiExtensionDisabledGlobalError::GetSeverity() {
  return SEVERITY_LOW;
}

bool VivaldiExtensionDisabledGlobalError::HasMenuItem() {
  // This error does not show up in any menus.
  return false;
}
int VivaldiExtensionDisabledGlobalError::MenuItemCommandID() {
  return command_id_;
}
std::u16string VivaldiExtensionDisabledGlobalError::MenuItemLabel() {
  std::string extension_name = external_install_error_->GetExtension()->name();
  return base::UTF8ToUTF16(extension_name);
}
void VivaldiExtensionDisabledGlobalError::ExecuteMenuItem(Browser* browser) {}

bool VivaldiExtensionDisabledGlobalError::HasBubbleView() {
  return false;
}
bool VivaldiExtensionDisabledGlobalError::HasShownBubbleView() {
  return false;
}
void VivaldiExtensionDisabledGlobalError::ShowBubbleView(Browser* browser) {
  if (external_install_error_) {
    //VivaldiBrowserComponentWrapper::GetInstance()->ShowExtensionErrorDialog(
    //    browser, external_install_error_.get());

     static_cast<ExternalInstallErrorDesktop*>(external_install_error_.get())
        ->ShowDialog(
            browser);

  } else if (disabled_upgrade_error_) {
    disabled_upgrade_error_->ShowBubbleView(browser);
  }
}

GlobalErrorBubbleViewBase*
VivaldiExtensionDisabledGlobalError::GetBubbleView() {
  return nullptr;
}

// GlobalErrorWithStandardBubble implementation.

void VivaldiExtensionDisabledGlobalError::OnShutdown(
    ExtensionRegistry* registry) {
  registry_observation_.Reset();
}

void VivaldiExtensionDisabledGlobalError::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (extension->id() == extension_id_) {
    RemoveGlobalError();
  }
}

void VivaldiExtensionDisabledGlobalError::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UninstallReason reason) {
  if (extension->id() == extension_id_) {
    RemoveGlobalError();
  }
}

void VivaldiExtensionDisabledGlobalError::RemoveGlobalError() {
  vivaldi::extension_action_utils::ExtensionInstallError error;
  error.id = extension_id_;
  error.name = extension_name_;
  SendGlobalErrorRemoved(&error);

  registry_observation_.Reset();

  //  VivaldiRootDocumentHandler* root_doc_handler =
  //      extensions::VivaldiRootDocumentHandlerFactory::GetForBrowserContext(
  //          browser_context_);

  // Avoid double deletes on shutdown.
  //  root_doc_handler->RemoveGlobalError(this);
}

const Extension* VivaldiExtensionDisabledGlobalError::GetExtension() {
  if (extension_)
    return extension_.get();

  const Extension* ext = external_install_error_
                             ? external_install_error_->GetExtension()
                             : nullptr;
  return ext;
}

ExtensionToIdProvider::ExtensionToIdProvider() {}

ExtensionToIdProvider::~ExtensionToIdProvider() {}

// Can be called multiple times.
int ExtensionToIdProvider::AddOrGetId(const std::string& extension_id) {
  int hasExtensionId = GetCommandId(extension_id);
  if (hasExtensionId != -1) {
    return hasExtensionId;
  }
  last_used_id_++;
  extension_ids_[extension_id] = last_used_id_;
  return last_used_id_;
}

void ExtensionToIdProvider::RemoveExtension(const std::string& extension_id) {
  DCHECK(GetCommandId(extension_id) == -1);
  extension_ids_.erase(extension_id);
}

int ExtensionToIdProvider::GetCommandId(const std::string& extension_id) {
  ExtensionToIdMap::const_iterator it = extension_ids_.find(extension_id);
  if (it == extension_ids_.end()) {
    return -1;
  }
  return it->second;
}

}  // namespace extensions
