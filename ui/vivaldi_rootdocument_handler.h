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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry_observer.h"
#include "ui/vivaldi_document_loader.h"

class PrefChangeRegistrar;
class VivaldiDocumentLoader;

namespace extensions {

class VivaldiRootDocumentHandler;

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
                                   public ProfileObserver {
  friend base::DefaultSingletonTraits<VivaldiRootDocumentHandler>;

 public:
  explicit VivaldiRootDocumentHandler(content::BrowserContext*);

  // ProfileObserver implementation.
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override;
  void OnProfileWillBeDestroyed(Profile* profile) override;

 private:
  ~VivaldiRootDocumentHandler() override;

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

  // These are the WebContents holders for our portal-windows. One document for
  // regular-windows and one for incognito-windows. Incognito is lazy loaded and
  // destroyed on the last private window closure.
  VivaldiDocumentLoader* vivaldi_document_loader_ = nullptr;
  VivaldiDocumentLoader* vivaldi_document_loader_off_the_record_ = nullptr;

  const Extension* vivaldi_extension_ = nullptr;
  // The profile we observe.
  Profile* profile_ = nullptr;
};

}  // namespace extensions

#endif
