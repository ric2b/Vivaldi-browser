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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "extensions/browser/extension_registry_observer.h"
#include "ui/infobar_container_web_proxy.h"
#include "ui/vivaldi_document_loader.h"

class PrefChangeRegistrar;
class VivaldiDocumentLoader;

namespace extensions {

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
  KeyedService* BuildServiceInstanceFor(
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
                                   public infobars::InfoBarContainer::Delegate {
  friend base::DefaultSingletonTraits<VivaldiRootDocumentHandler>;

 public:
  explicit VivaldiRootDocumentHandler(content::BrowserContext*);

  void AddObserver(VivaldiRootDocumentHandlerObserver* observer);
  void RemoveObserver(VivaldiRootDocumentHandlerObserver* observer);

  content::WebContents* GetWebContents();
  content::WebContents* GetOTRWebContents();

  raw_ptr<::vivaldi::InfoBarContainerWebProxy> InfoBarContainer() {
    return infobar_container_.get();
  }

 private:
  ~VivaldiRootDocumentHandler() override;

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
};

}  // namespace extensions

#endif
