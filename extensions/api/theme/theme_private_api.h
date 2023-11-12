// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_THEME_THEME_PRIVATE_API_H_
#define EXTENSIONS_API_THEME_THEME_PRIVATE_API_H_

#include "base/files/file_path.h"
#include "base/values.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"

#include "components/datasource/vivaldi_theme_io.h"
#include "components/theme/vivaldi_theme_download.h"
#include "extensions/schema/theme_private.h"

class Profile;

namespace extensions {

class ThemePrivateAPI : public BrowserContextKeyedAPI {
  friend class BrowserContextKeyedAPIFactory<ThemePrivateAPI>;

 public:
  explicit ThemePrivateAPI(content::BrowserContext* context);
  ~ThemePrivateAPI() override;
  ThemePrivateAPI(const ThemePrivateAPI&) = delete;
  ThemePrivateAPI& operator=(const ThemePrivateAPI&) = delete;

  static ThemePrivateAPI* FromBrowserContext(
      content::BrowserContext* browser_context);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<ThemePrivateAPI>* GetFactoryInstance();

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "ThemePrivateAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

 private:
  void OnPrefsChanged(const std::string& path);

  const raw_ptr<Profile> profile_;
  PrefChangeRegistrar prefs_registrar_;
};

class ThemePrivateExportFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("themePrivate.export", THEMEPRIVATE_EXPORT)
  ThemePrivateExportFunction() = default;

 private:
  ~ThemePrivateExportFunction() override = default;
  ResponseAction Run() override;

  void StartExport(base::FilePath theme_archive);
  void OnFileSelectionDone(base::FilePath theme_archive, bool cancelled);
  void SendResult(std::vector<uint8_t> dataBlob, bool success);

  base::Value theme_object_;
};

class ThemePrivateImportFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("themePrivate.import", THEMEPRIVATE_IMPORT)
  ThemePrivateImportFunction() = default;

 private:
  ~ThemePrivateImportFunction() override = default;
  ResponseAction Run() override;

  void StartImport(std::vector<uint8_t> archive_data);
  void SendResult(std::string theme_id,
                  std::unique_ptr<vivaldi_theme_io::ImportError> error);
};

class ThemePrivateDownloadFunction
    : public ExtensionFunction,
      public ::vivaldi::VivaldiThemeDownloadHelper::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("themePrivate.download", THEMEPRIVATE_DOWNLOAD)
  ThemePrivateDownloadFunction();

 private:
  ~ThemePrivateDownloadFunction() override;

  // ExtensionFunction

  ResponseAction Run() override;

  // VivaldiThemeDownloadHelper::Delegate

  void DownloadStarted(const std::string& theme_id) override;
  void DownloadProgress(const std::string& theme_id, uint64_t current) override;
  void DownloadCompleted(const std::string& theme_id,
                         bool success,
                         std::string error_msg) override;

  void SendResult(std::string theme_id,
                  std::unique_ptr<vivaldi_theme_io::ImportError> error);

  std::unique_ptr<::vivaldi::VivaldiThemeDownloadHelper> download_helper_;
};

class ThemePrivateGetThemeDataFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("themePrivate.getThemeData",
                             THEMEPRIVATE_GETTHEMEDATA)
  ThemePrivateGetThemeDataFunction() = default;

 private:
  ~ThemePrivateGetThemeDataFunction() override;
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_THEME_THEME_PRIVATE_API_H_
