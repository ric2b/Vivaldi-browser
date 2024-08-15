// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/theme/theme_private_api.h"

#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "extensions/helper/file_selection_options.h"
#include "net/base/filename_util.h"

#include "extensions/tools/vivaldi_tools.h"
#include "prefs/vivaldi_gen_prefs.h"
#include "ui/vivaldi_browser_window.h"

namespace extensions {

// static
ThemePrivateAPI* ThemePrivateAPI::FromBrowserContext(
    content::BrowserContext* browser_context) {
  ThemePrivateAPI* api = GetFactoryInstance()->Get(browser_context);
  DCHECK(api);
  return api;
}

// static
BrowserContextKeyedAPIFactory<ThemePrivateAPI>*
ThemePrivateAPI::GetFactoryInstance() {
  static base::NoDestructor<BrowserContextKeyedAPIFactory<ThemePrivateAPI>>
      instance;
  return instance.get();
}

ThemePrivateAPI::ThemePrivateAPI(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  prefs_registrar_.Init(profile_->GetPrefs());
  // Unretained is safe as it follows the lifetime of the registrar.
  prefs_registrar_.Add(vivaldiprefs::kThemesPreview,
                       base::BindRepeating(&ThemePrivateAPI::OnPrefsChanged,
                                           base::Unretained(this)));
  prefs_registrar_.Add(vivaldiprefs::kThemesUser,
                       base::BindRepeating(&ThemePrivateAPI::OnPrefsChanged,
                                           base::Unretained(this)));
}

void ThemePrivateAPI::OnPrefsChanged(const std::string& path) {
  if (path == vivaldiprefs::kThemesPreview ||
      path == vivaldiprefs::kThemesUser) {
    ::vivaldi::BroadcastEvent(
        extensions::vivaldi::theme_private::OnThemesUpdated::kEventName,
        extensions::vivaldi::theme_private::OnThemesUpdated::Create(
            path == vivaldiprefs::kThemesPreview),
        profile_);
  }
}

ThemePrivateAPI::~ThemePrivateAPI() {}

ExtensionFunction::ResponseAction ThemePrivateExportFunction::Run() {
  using extensions::vivaldi::theme_private::Export::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  theme_object_ = base::Value(params->theme.ToValue());
  std::string error;
  vivaldi_theme_io::VerifyAndNormalizeJson(
      {.for_export = true, .allow_named_id = true}, theme_object_, error);
  if (!error.empty()) {
    return RespondNow(Error(error));
  }
  if (params->options.return_blob && *params->options.return_blob) {
    if (params->options.dialog_title || params->options.window_id) {
      return RespondNow(
          Error("File choice related options should not given if returnBlob is "
                "specified"));
    }
    StartExport(base::FilePath());
  } else {
    if (!params->options.dialog_title ||
        params->options.dialog_title->empty() || !params->options.window_id) {
      return RespondNow(
          Error("Both dialogTitle and windowId must be specified"));
    }
    base::FilePath theme_filename =
        base::FilePath::FromUTF8Unsafe(*theme_object_.GetDict().FindString("name"));
    net::GenerateSafeFileName("application/zip", /*ignore_extension=*/true,
                              &theme_filename);

    FileSelectionOptions options(*params->options.window_id);
    options.SetType(ui::SelectFileDialog::SELECT_SAVEAS_FILE);
    options.SetTitle(*params->options.dialog_title);
    options.AddExtension("zip");

    std::move(options).RunDialog(
        base::BindOnce(&ThemePrivateExportFunction::OnFileSelectionDone, this));
  }

  return RespondLater();
}

void ThemePrivateExportFunction::StartExport(base::FilePath theme_archive) {
  vivaldi_theme_io::Export(
      browser_context(), std::move(theme_object_), std::move(theme_archive),
      base::BindOnce(&ThemePrivateExportFunction::SendResult, this));
}

void ThemePrivateExportFunction::OnFileSelectionDone(
    base::FilePath theme_archive,
    bool cancell) {
  if (theme_archive.empty()) {
    SendResult(std::vector<uint8_t>(), /*success=*/false);
    return;
  }
  StartExport(std::move(theme_archive));
}

void ThemePrivateExportFunction::SendResult(std::vector<uint8_t> dataBlob,
                                            bool success) {
  namespace Results = vivaldi::theme_private::Export::Results;
  using extensions::vivaldi::theme_private::ExportResult;

  ExportResult result;
  result.success = success;
  if (!dataBlob.empty()) {
    result.data_blob = std::move(dataBlob);
  }
  Respond(ArgumentList(Results::Create(result)));
}

ExtensionFunction::ResponseAction ThemePrivateImportFunction::Run() {
  using extensions::vivaldi::theme_private::Import::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  StartImport(std::move(params->options.data_blob));
  return RespondLater();
}

void ThemePrivateImportFunction::StartImport(
    std::vector<uint8_t> archive_data) {
  vivaldi_theme_io::Import(
      Profile::FromBrowserContext(browser_context())->GetWeakPtr(),
      base::FilePath(),
      std::move(archive_data),
      base::BindOnce(&ThemePrivateImportFunction::SendResult, this));
}

namespace {
using extensions::vivaldi::theme_private::ImportResult;

ImportResult CreateImportResult(
    std::string theme_id,
    std::unique_ptr<vivaldi_theme_io::ImportError> error) {
  using extensions::vivaldi::theme_private::ImportError;
  using extensions::vivaldi::theme_private::ImportErrorKind;

  DCHECK(theme_id.empty() || !error);

  ImportResult result;
  result.theme_id = std::move(theme_id);
  if (error) {
    result.error = ImportError();

    switch (error->kind) {
      case vivaldi_theme_io::ImportError::kIO:
        result.error->kind = ImportErrorKind::kIo;
        break;
      case vivaldi_theme_io::ImportError::kBadArchive:
        result.error->kind = ImportErrorKind::kBadArchive;
        break;
      case vivaldi_theme_io::ImportError::kBadSettings:
        result.error->kind = ImportErrorKind::kBadSettings;
        break;
    }
    result.error->details = std::move(error->details);
  }
  return result;
}

}  // namespace

void ThemePrivateImportFunction::SendResult(
    std::string theme_id,
    std::unique_ptr<vivaldi_theme_io::ImportError> error) {
  namespace Results = vivaldi::theme_private::Import::Results;
  using extensions::vivaldi::theme_private::ImportResult;

  ImportResult result = CreateImportResult(theme_id, std::move(error));

  Respond(ArgumentList(Results::Create(result)));
}

ThemePrivateDownloadFunction::ThemePrivateDownloadFunction() {}
ThemePrivateDownloadFunction::~ThemePrivateDownloadFunction() {}

void ThemePrivateDownloadFunction::DownloadStarted(
    const std::string& theme_id) {
  ::vivaldi::BroadcastEvent(
      extensions::vivaldi::theme_private::OnThemeDownloadStarted::kEventName,
      extensions::vivaldi::theme_private::OnThemeDownloadStarted::Create(
          theme_id),
      browser_context());
}
void ThemePrivateDownloadFunction::DownloadProgress(const std::string& theme_id,
                                                    uint64_t current) {
  ::vivaldi::BroadcastEvent(
      extensions::vivaldi::theme_private::OnThemeDownloadProgress::kEventName,
      extensions::vivaldi::theme_private::OnThemeDownloadProgress::Create(
          theme_id, current),
      browser_context());
}
void ThemePrivateDownloadFunction::DownloadCompleted(
    const std::string& theme_id,
    bool success,
    std::string error_msg) {
  ::vivaldi::BroadcastEvent(
      extensions::vivaldi::theme_private::OnThemeDownloadCompleted::kEventName,
      extensions::vivaldi::theme_private::OnThemeDownloadCompleted::Create(
          theme_id, success, error_msg),
      browser_context());
}

ExtensionFunction::ResponseAction ThemePrivateDownloadFunction::Run() {
  using extensions::vivaldi::theme_private::Download::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GURL url(params->url);

  download_helper_ = std::make_unique<::vivaldi::VivaldiThemeDownloadHelper>(
      std::move(params->theme_id), url,
      base::BindOnce(&ThemePrivateDownloadFunction::SendResult, this),
      Profile::FromBrowserContext(browser_context())->GetWeakPtr());
  download_helper_->set_delegate(this);
  download_helper_->DownloadAndInstall();

  return RespondLater();
}

void ThemePrivateDownloadFunction::SendResult(
    std::string theme_id,
    std::unique_ptr<vivaldi_theme_io::ImportError> error) {
  namespace Results = vivaldi::theme_private::Download::Results;
  using extensions::vivaldi::theme_private::ImportResult;

  ImportResult result = CreateImportResult(theme_id, std::move(error));

  Respond(ArgumentList(Results::Create(result)));
}

ThemePrivateGetThemeDataFunction::~ThemePrivateGetThemeDataFunction() {}

ExtensionFunction::ResponseAction ThemePrivateGetThemeDataFunction::Run() {
  using extensions::vivaldi::theme_private::GetThemeData::Params;
  namespace Results = vivaldi::theme_private::GetThemeData::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  PrefService* prefs = profile->GetPrefs();

  double version = vivaldi_theme_io::FindVersionByThemeId(prefs, params->id);

  vivaldi::theme_private::ThemeData retval;

  retval.id = params->id;
  retval.is_installed = (version != 0.0);
  retval.version = version;  // + 1.0; // DEBUG to test Update

  return RespondNow(ArgumentList(Results::Create(std::move(retval))));
}

}  // namespace extensions
