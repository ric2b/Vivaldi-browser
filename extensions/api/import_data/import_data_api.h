// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_IMPORT_DATA_IMPORT_DATA_API_H_
#define EXTENSIONS_API_IMPORT_DATA_IMPORT_DATA_API_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/importer/importer_progress_observer.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/importer/importer_type.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
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

// Observes import process and then routes the notifications as events to
// the extension system.
class ImportDataEventRouter {
 public:
  explicit ImportDataEventRouter(Profile* profile);
  ~ImportDataEventRouter();

  // Helper to actually dispatch an event to extension listeners.
  void DispatchEvent(const std::string& event_name,
                     std::unique_ptr<base::ListValue> event_args);

 private:
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(ImportDataEventRouter);
};

class ImportDataAPI : public importer::ImporterProgressObserver,
                      public BrowserContextKeyedAPI,
                      public EventRouter::Observer {
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

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<ImportDataAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<ImportDataAPI>;

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "ImportDataAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<ImportDataEventRouter> event_router_;

  // If non-null it means importing is in progress. ImporterHost takes care
  // of deleting itself when import is complete.
  ExternalProcessImporterHost* importer_host_;

  // Keeps count of successful imported items. If not 0 when import ends, one
  // import failed.  Lousy error checking in Chromium forces this.
  int import_succeeded_count_;
};

class ImporterApiFunction : public ChromeAsyncExtensionFunction {
 public:
  ImporterApiFunction();
  // AsyncExtensionFunction:
  virtual void SendAsyncResponse();
  ImporterList* api_importer_list;
  virtual void Finished();

 protected:
  ~ImporterApiFunction() override;

  bool RunAsync() override;

  virtual void SendResponseToCallback();
  virtual bool RunAsyncImpl() = 0;
};

class ImportDataGetProfilesFunction : public ImporterApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("importData.getProfiles", IMPORTDATA_GETPROFILES)
  ImportDataGetProfilesFunction();

 protected:
  ~ImportDataGetProfilesFunction() override;
  bool RunAsyncImpl() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImportDataGetProfilesFunction);
};

class ImportDataStartImportFunction : public ImporterApiFunction,
                                      public ui::SelectFileDialog::Listener {
 public:
  DECLARE_EXTENSION_FUNCTION("importData.startImport", IMPORTDATA_STARTIMPORT)
  ImportDataStartImportFunction();

 protected:
  ~ImportDataStartImportFunction() override;

  struct DialogParams {
    DialogParams()
        : imported_items(0),
          importer_type(importer::TYPE_UNKNOWN),
          file_dialog(true) {}

    // Items to import
    int imported_items;

    // The importer type, need to select correct dialog
    importer::ImporterType importer_type;

    // Is this a file or folder dialog?
    bool file_dialog;
  };

  // ExtensionFunction:
  bool RunAsyncImpl() override;

  // ui::SelectFileDialog::Listener:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectionCanceled(void* params) override;

  void StartImport(const importer::SourceProfile& source_profile,
                   uint16_t imported_items);
  void ImportData(const base::ListValue* args);

  void HandleChooseBookmarksFileOrFolder(const base::string16& title,
                                         const std::string& extension,
                                         int imported_items,
                                         importer::ImporterType importer_type,
                                         const base::FilePath& default_file,
                                         bool file_selection);

 private:
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  DISALLOW_COPY_AND_ASSIGN(ImportDataStartImportFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_IMPORT_DATA_IMPORT_DATA_API_H_
