#include "chromium_extension_importer.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/webstore_install_with_prompt.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_data_importer/common/importer_data_types.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/vivaldi_silent_extension_installer.h"

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
    return extensions->Clone();
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

ChromiumExtensionsImporter::ChromiumExtensionsImporter(Profile* profile)
    : profile_(profile) {}

ChromiumExtensionsImporter::~ChromiumExtensionsImporter() = default;

void ChromiumExtensionsImporter::OnExtensionAdded(
    bool success,
    const std::string& error,
    extensions::webstore_install::Result result) {}

void ChromiumExtensionsImporter::AddExtensions(
    const std::vector<std::string> extensions) {
  using namespace extensions;
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  if (!registry) {
    return;
  }
  for (const auto& extension : extensions) {
    // Skip if extension is already installed or blocklisted.
    const Extension* installed_extension = registry->GetExtensionById(
        extension, ExtensionRegistry::ENABLED | ExtensionRegistry::DISABLED |
                       ExtensionRegistry::BLOCKLISTED);
    if (installed_extension) {
      continue;
    }

    scoped_refptr<vivaldi::SilentWebstoreInstaller> installer =
        new vivaldi::SilentWebstoreInstaller(
            extension, profile_, gfx::NativeWindow(),
            base::BindOnce(&ChromiumExtensionsImporter::OnExtensionAdded,
                           weak_ptr_factory_.GetWeakPtr()));
    installer->BeginInstall();
  }
}

}  // namespace extension_importer
