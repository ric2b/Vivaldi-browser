// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef EXTRAPARTS_VIVALDI_SILENT_EXTENSION_INSTALLER_H
#define EXTRAPARTS_VIVALDI_SILENT_EXTENSION_INSTALLER_H

#include "chrome/browser/extensions/webstore_install_with_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/extension_system.h"

namespace vivaldi {

class SilentWebstoreInstaller : public extensions::WebstoreInstallWithPrompt {
 public:
  using WebstoreInstallWithPrompt::WebstoreInstallWithPrompt;

  static void Install(const std::string &id,
                      Profile* profile,
                      Callback callback) {
    scoped_refptr<SilentWebstoreInstaller> installer =
        new SilentWebstoreInstaller(id, profile, gfx::NativeWindow(),
                                    std::move(callback));
    installer->BeginInstall();
  }

  static void SetExtensionEnabled(const std::string& id,
                                  bool enabled,
                                  Profile* profile) {
    extensions::ExtensionRegistry* registry = extensions::ExtensionRegistry::Get(profile);

    const extensions::Extension* installed_extension = registry->GetExtensionById(
        id, extensions::ExtensionRegistry::ENABLED | extensions::ExtensionRegistry::DISABLED |
            extensions::ExtensionRegistry::BLOCKLISTED);
    if (installed_extension) {
      extensions::ExtensionRegistrar* extension_registrar =
            extensions::ExtensionRegistrar::Get(profile);
      DCHECK(extension_registrar);
      if (enabled) {
          extension_registrar->EnableExtension(id);
      } else {
        extension_registrar->DisableExtension(
            id, {extensions::disable_reason::DISABLE_USER_ACTION});
      }
    }
  }

 private:
  ~SilentWebstoreInstaller() override = default;

  std::unique_ptr<ExtensionInstallPrompt::Prompt> CreateInstallPrompt()
      const override {
    return nullptr;
  }
  bool ShouldShowPostInstallUI() const override { return false; }

  void CompleteInstall(extensions::webstore_install::Result result,
                       const std::string& error) override {
    this->set_show_post_install_ui(false); // Don't show post-install ui.
    if (result == extensions::webstore_install::SUCCESS) {
      extensions::ExtensionSystem* system =
          extensions::ExtensionSystem::Get(profile());
      if (!system || !system->extension_service()) {
        extensions::WebstoreInstallWithPrompt::CompleteInstall(result, error);
        return;
      }

#if 0
      extensions::ExtensionService* service = system->extension_service();
      service->DisableExtension(
          id(), extensions::disable_reason::DISABLE_USER_ACTION);
#endif
    }
    extensions::WebstoreInstallWithPrompt::CompleteInstall(result, error);
  }
};

} // namespace vivaldi

#endif /* EXTRAPARTS_VIVALDI_SILENT_EXTENSION_INSTALLER_H */
