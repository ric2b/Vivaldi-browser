// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_IMPORT_DATA_IMPORT_DATA_API_H_
#define EXTENSIONS_API_IMPORT_DATA_IMPORT_DATA_API_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/importer/importer_progress_observer.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/importer/importer_type.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class ImporterList;
class ExternalProcessImporterHost;

namespace extensions {

// Singleton that owns all ProfileModels
class ProfileSingletonFactory {
 public:
  static ProfileSingletonFactory* getInstance();
  ImporterList* getImporterList();
  bool getProfileRequested();
  void setProfileRequested(bool profileReq);
  ~ProfileSingletonFactory();

 private:
  bool profilesRequested;
  static bool instanceFlag;
  static ProfileSingletonFactory* single;
  std::unique_ptr<ImporterList> api_importer_list_;
  ProfileSingletonFactory();

  DISALLOW_COPY_AND_ASSIGN(ProfileSingletonFactory);
};

class ImportDataAPI : public importer::ImporterProgressObserver,
                      public BrowserContextKeyedAPI {
 public:
  explicit ImportDataAPI(content::BrowserContext* context);
  ~ImportDataAPI() override;

  void StartImport(const importer::SourceProfile& source_profile,
                   uint16_t imported_items);

  // importer::ImporterProgressObserver:
  void ImportStarted() override;
  void ImportItemStarted(importer::ImportItem item) override;
  void ImportItemEnded(importer::ImportItem item) override;
  void ImportEnded() override;
  void ImportItemFailed(importer::ImportItem item,
                        const std::string& error) override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<ImportDataAPI>* GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<ImportDataAPI>;

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "ImportDataAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // If non-null it means importing is in progress. ImporterHost takes care
  // of deleting itself when import is complete.
  ExternalProcessImporterHost* importer_host_;

  // Keeps count of successful imported items. If not 0 when import ends, one
  // import failed.  Lousy error checking in Chromium forces this.
  int import_succeeded_count_;
};

class ImportDataGetProfilesFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("importData.getProfiles", IMPORTDATA_GETPROFILES)
  ImportDataGetProfilesFunction();

 private:
  ~ImportDataGetProfilesFunction() override;
  ResponseAction Run() override;

  void Finished();

  ImporterList* api_importer_list_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ImportDataGetProfilesFunction);
};

class ImportDataStartImportFunction : public UIThreadExtensionFunction,
                                      public ui::SelectFileDialog::Listener {
 public:
  DECLARE_EXTENSION_FUNCTION("importData.startImport", IMPORTDATA_STARTIMPORT)
  ImportDataStartImportFunction();

 private:
  ~ImportDataStartImportFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  // ui::SelectFileDialog::Listener:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectionCanceled(void* params) override;

  void StartImport(const importer::SourceProfile& source_profile);
  void ImportData(const base::ListValue* args);

  ResponseAction HandleChooseBookmarksFileOrFolder(
      const base::string16& title,
      base::StringPiece extension,
      const base::FilePath& default_file,
      bool file_selection);

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  // Items to import
  int imported_items_ = 0;

  // The importer type, need to select correct dialog
  importer::ImporterType importer_type_ = importer::TYPE_UNKNOWN;

  DISALLOW_COPY_AND_ASSIGN(ImportDataStartImportFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_IMPORT_DATA_IMPORT_DATA_API_H_
