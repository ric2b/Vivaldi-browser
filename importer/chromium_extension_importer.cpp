#include "chromium_extension_importer.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/webstore_install_with_prompt.h"
#include "chrome/browser/importer/external_process_importer_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/importer/importer_data_types.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"

#include <optional>
#include <string>

namespace {
using extensions::Extension;
using extensions::Manifest;

inline constexpr char kChromeExtensionsListPath[] = "extensions.settings";
inline constexpr char kChromeSecurePreferencesFile[] = "Secure Preferences";
inline constexpr char kChromePreferencesFile[] = "Preferences";

base::Value::Dict GetExtensionsFromPreferences(const base::FilePath& path) {
  if (!base::PathExists(path))
    return base::Value::Dict();

  std::string preference_content;
  base::ReadFileToString(path, &preference_content);

  std::optional<base::Value> preference =
      base::JSONReader::Read(preference_content);
  DCHECK(preference);
  DCHECK(preference->is_dict());
  if (auto* extensions = preference->GetDict().FindDictByDottedPath(
          kChromeExtensionsListPath)) {
    return std::move(*extensions);
  }
  return base::Value::Dict();
}

base::Value::Dict GetChromiumExtensions(const base::FilePath& profile_dir) {
  auto secure_preferences = GetExtensionsFromPreferences(
      profile_dir.AppendASCII(kChromeSecurePreferencesFile));

  auto preferences = GetExtensionsFromPreferences(
      profile_dir.AppendASCII(kChromePreferencesFile));

  secure_preferences.Merge(std::move(preferences));
  return secure_preferences;
}

std::vector<std::string> FilterImportableExtensions(
    const base::Value::Dict& extensions_list) {
  std::vector<std::string> extensions;
  for (const auto [key, value] : extensions_list) {
    DCHECK(value.is_dict());
    const base::Value::Dict& dict = value.GetDict();
    // Do not import extensions installed by default
    if (dict.FindBool("was_installed_by_default").value_or(true)) {
      continue;
    }
    // Nor disabled extensions
    if (!dict.FindInt("state").value_or(false)) {
      continue;
    }

    // Nor extensions not installed from webstore
    if (!dict.FindBool("from_webstore").value_or(false)) {
      continue;
    }

    // Install only type extension
    if (auto* manifest_dict = dict.FindDict("manifest")) {
      if (Manifest::GetTypeFromManifestValue(*manifest_dict) ==
          Manifest::TYPE_EXTENSION) {
        extensions.push_back(key);
      }
    }
  }
  return extensions;
}

class SilentWebstoreInstaller : public extensions::WebstoreInstallWithPrompt {
 public:
  using WebstoreInstallWithPrompt::WebstoreInstallWithPrompt;

 private:
  ~SilentWebstoreInstaller() override = default;

  std::unique_ptr<ExtensionInstallPrompt::Prompt> CreateInstallPrompt()
      const override {
    return nullptr;
  }
  bool ShouldShowPostInstallUI() const override { return false; }

  void CompleteInstall(extensions::webstore_install::Result result,
                       const std::string& error) override {
    if (result == extensions::webstore_install::SUCCESS) {
      extensions::ExtensionSystem* system =
          extensions::ExtensionSystem::Get(profile());
      if (!system || !system->extension_service()) {
        extensions::WebstoreInstallWithPrompt::CompleteInstall(result, error);
        return;
      }

      extensions::ExtensionService* service = system->extension_service();
      service->DisableExtension(
          id(), extensions::disable_reason::DISABLE_USER_ACTION);
    }
    extensions::WebstoreInstallWithPrompt::CompleteInstall(result, error);
  }
};
}  // namespace

namespace extension_importer {

std::vector<std::string> ChromiumExtensionsImporter::GetImportableExtensions(
    const base::FilePath& profile_dir) {
  return FilterImportableExtensions(GetChromiumExtensions(profile_dir));
}

bool ChromiumExtensionsImporter::CanImportExtensions(
    const base::FilePath& profile_dir) {
  return !GetImportableExtensions(profile_dir).empty();
}

ChromiumExtensionsImporter::ChromiumExtensionsImporter(
    Profile* profile,
    base::WeakPtr<ExternalProcessImporterHost> host)
    : profile_(profile), host_(host) {}

ChromiumExtensionsImporter::~ChromiumExtensionsImporter() = default;

void ChromiumExtensionsImporter::OnExtensionAdded(
    bool success,
    const std::string& error,
    extensions::webstore_install::Result result) {
  if (host_ && !success) {
    host_->NotifyImportItemFailed(importer::EXTENSIONS, error);
  }
  FinishExtensionProcessing();
}

void ChromiumExtensionsImporter::AddExtensions(
    const std::vector<std::string>& extensions) {
  using namespace extensions;
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  DCHECK(registry);
  extensions_size_ = extensions.size();
  for (const auto& extension : extensions) {
    // Skip if extension is already installed or blocklisted.
    const Extension* installed_extension = registry->GetExtensionById(
        extension, ExtensionRegistry::ENABLED | ExtensionRegistry::DISABLED |
                       ExtensionRegistry::BLOCKLISTED);
    if (installed_extension) {
      FinishExtensionProcessing();
      continue;
    }

    scoped_refptr<SilentWebstoreInstaller> installer =
        new SilentWebstoreInstaller(
            extension, profile_, nullptr,
            base::BindOnce(&ChromiumExtensionsImporter::OnExtensionAdded,
                           weak_ptr_factory_.GetWeakPtr()));
    installer->BeginInstall();
  }
}

void ChromiumExtensionsImporter::FinishExtensionProcessing() {
  if (++extensions_processed_ >= extensions_size_ && host_) {
    host_->NotifyImportItemEnded(importer::EXTENSIONS);
    host_->NotifyImportEnded();
  }
}

}  // namespace extension_importer
