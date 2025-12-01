// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef VIVALDI_ROOT_DOCUMENT_PROFILE_HANDLER_H_
#define VIVALDI_ROOT_DOCUMENT_PROFILE_HANDLER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "app/vivaldi_constants.h"

#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/extensions/external_install_error.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/schema/browser_action_utilities.h"
#include "ui/infobar_container_web_proxy.h"
#include "ui/vivaldi_document_loader.h"

class PrefChangeRegistrar;
class VivaldiDocumentLoader;

namespace extensions {

class ExtensionService;
class VivaldiExtensionDisabledGlobalError;

using ExtensionToIdMap = base::flat_map<std::string, int>;

// Helper class to map extension id to an unique id used for each error.
class ExtensionToIdProvider {
 public:
  ExtensionToIdProvider();

  ExtensionToIdProvider(const ExtensionToIdProvider&) = delete;
  ExtensionToIdProvider& operator=(const ExtensionToIdProvider&) = delete;

  ~ExtensionToIdProvider();

  int AddOrGetId(const std::string& extension_id);
  void RemoveExtension(const std::string& extension_id);

  // Used to look up a GlobalError through GetGlobalErrorByMenuItemCommandID.
  // Returns -1 on not-found.
  int GetCommandId(const std::string& extension_id);

 private:
  // Counted unique id.
  int last_used_id_ = 0;

  ExtensionToIdMap extension_ids_;
};

class VivaldiRootDocumentHandler;

void MarkProfilePathForNoVivaldiClient(const base::FilePath& path);
void ClearProfilePathForNoVivaldiClient(const base::FilePath& path);
bool ProfileShouldNotUseVivaldiClient(const base::FilePath& path);

class VivaldiRootDocumentHandlerObserver {
 public:
  virtual ~VivaldiRootDocumentHandlerObserver() {}

  // Called when the root document has finished loading.
  virtual void OnRootDocumentDidFinishNavigation() {}

  // Return the corresponding webcontents. Used for loaded state on
  // observe-start.
  virtual content::WebContents* GetRootDocumentWebContents() = 0;
};

class VivaldiRootDocumentHandlerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static VivaldiRootDocumentHandler* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static VivaldiRootDocumentHandlerFactory* GetInstance();

 private:
  // Friend NoDestructor to permit access to the private constructor.
  friend class base::NoDestructor<VivaldiRootDocumentHandlerFactory>;

  VivaldiRootDocumentHandlerFactory();
  ~VivaldiRootDocumentHandlerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

class VivaldiRootDocumentHandler : public KeyedService,
                                   public extensions::ExtensionRegistryObserver,
                                   public ProfileObserver,
                                   protected content::WebContentsObserver,
                                   public infobars::InfoBarContainer::Delegate,
                                   public CommandService::Observer {
  friend base::DefaultSingletonTraits<VivaldiRootDocumentHandler>;

 public:
  explicit VivaldiRootDocumentHandler(content::BrowserContext*);
  ~VivaldiRootDocumentHandler() override;

  void AddObserver(VivaldiRootDocumentHandlerObserver* observer);
  void RemoveObserver(VivaldiRootDocumentHandlerObserver* observer);

  content::WebContents* GetWebContents();
  content::WebContents* GetOTRWebContents();

  raw_ptr<::vivaldi::InfoBarContainerWebProxy> InfoBarContainer() {
    return infobar_container_.get();
  }
///
  void AddGlobalError(
      std::unique_ptr<VivaldiExtensionDisabledGlobalError> error);

  void RemoveGlobalError(VivaldiExtensionDisabledGlobalError* error);

  raw_ptr<VivaldiExtensionDisabledGlobalError>
  GetGlobalErrorByMenuItemCommandID(int commandid);

  std::vector<std::unique_ptr<VivaldiExtensionDisabledGlobalError>>& errors() {
    return errors_;
  }

  ExtensionToIdProvider& GetExtensionToIdProvider() { return id_provider_; }

  //
 private:
  // ProfileObserver implementation.
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override;
  void OnProfileWillBeDestroyed(Profile* profile) override;

  class DocumentContentsObserver : public content::WebContentsObserver {
   public:
    DocumentContentsObserver(VivaldiRootDocumentHandler* router,
                             content::WebContents* contents);

    // content::WebContentsObserver overrides.
    void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override;

   private:
    raw_ptr<VivaldiRootDocumentHandler> root_doc_handler_;
  };

  // Overridden from CommandService::Observer
  void OnExtensionCommandAdded(const std::string& extension_id,
                               const Command& added_command) override;
  void OnExtensionCommandRemoved(const std::string& extension_id,
                                 const Command& removed_command) override;

  // KeyedService implementation.
  void Shutdown() override;

  // Overridden from extensions::ExtensionRegistryObserver:
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const extensions::Extension* extension,
                            bool is_update) override;

  // infobars::InfoBarContainer::Delegate.

  void InfoBarContainerStateChanged(bool is_animating) override {}

  void InformObservers();

  // These are the WebContents holders for our portal-windows. One document for
  // regular-windows and one for incognito-windows. Incognito is lazy loaded and
  // destroyed on the last private window closure.
  raw_ptr<VivaldiDocumentLoader> vivaldi_document_loader_ = nullptr;
  raw_ptr<VivaldiDocumentLoader> vivaldi_document_loader_off_the_record_ =
      nullptr;

  // Observer handlers for the webcontents owned by the two
  // VivaldiDocumentLoaders.
  std::unique_ptr<DocumentContentsObserver> document_observer_;
  std::unique_ptr<DocumentContentsObserver> otr_document_observer_;

  bool document_loader_is_ready_ = false;
  bool otr_document_loader_is_ready_ = false;

  base::ObserverList<VivaldiRootDocumentHandlerObserver>::Unchecked observers_;

  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      observed_profiles_{this};

  // The InfoBarContainerWebProxy that contains InfoBars for the current tab.
  std::unique_ptr<::vivaldi::InfoBarContainerWebProxy> infobar_container_;

  raw_ptr<const Extension> vivaldi_extension_ = nullptr;
  // The profile we observe.
  const raw_ptr<Profile> profile_ = nullptr;

  // Lookup between extension id and command-id.
  ExtensionToIdProvider id_provider_;

  // Owned global errors.
  std::vector<std::unique_ptr<VivaldiExtensionDisabledGlobalError>> errors_;
};

// Used to show ui in Vivaldi for ExternalInstallBubbleAlert and
// ExtensionDisabledGlobalError.
class VivaldiExtensionDisabledGlobalError
    : public GlobalError,
      public ExtensionUninstallDialog::Delegate,
      public ExtensionRegistryObserver {
 public:
  // ExtensionDisabledGlobalError
  VivaldiExtensionDisabledGlobalError(
      ExtensionService* service,
      const Extension* extension,
      base::WeakPtr<GlobalErrorWithStandardBubble> disabled_upgrade_error);

  // ExtensionDisabledGlobalError
  VivaldiExtensionDisabledGlobalError(
      content::BrowserContext* context,
      base::WeakPtr<ExternalInstallError> error);

  ~VivaldiExtensionDisabledGlobalError() override;

  const Extension* GetExtension();
  const std::string& GetExtensionId() { return extension_id_; }
  const std::string& GetExtensionName() { return extension_name_; }

  void SendGlobalErrorRemoved(
      vivaldi::extension_action_utils::ExtensionInstallError* jserror);

  static void SendGlobalErrorRemoved(
      content::BrowserContext* browser_context,
      vivaldi::extension_action_utils::ExtensionInstallError* jserror);

  // GlobalError implementation.
  int MenuItemCommandID() override;

 private:
  // GlobalError implementation.
  Severity GetSeverity() override;
  bool HasMenuItem() override;
  std::u16string MenuItemLabel() override;
  void ExecuteMenuItem(Browser* browser) override;
  bool HasBubbleView() override;
  bool HasShownBubbleView() override;
  void ShowBubbleView(Browser* browser) override;

  GlobalErrorBubbleViewBase* GetBubbleView() override;
  void SendGlobalErrorAdded(
      vivaldi::extension_action_utils::ExtensionInstallError* jserror);

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              UninstallReason reason) override;
  void OnShutdown(ExtensionRegistry* registry) override;

  void RemoveGlobalError();

  content::BrowserContext* browser_context_;
  raw_ptr<ExtensionService, DanglingUntriaged> service_;
  scoped_refptr<const Extension> extension_;

  // ExtensionDisabledGlobalError, owned by GlobalErrorService.
  base::WeakPtr<GlobalErrorWithStandardBubble> disabled_upgrade_error_;

  base::WeakPtr<ExternalInstallError> external_install_error_;

  // Copy of the Extension values in case we delete the extension.
  std::string extension_id_;
  std::string extension_name_;
  int command_id_;  // Used to lookup error via errorservice or utils.

  std::unique_ptr<ExtensionUninstallDialog> uninstall_dialog_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};
};

}  // namespace extensions

#endif
