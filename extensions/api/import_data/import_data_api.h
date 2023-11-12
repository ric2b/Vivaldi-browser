// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_IMPORT_DATA_IMPORT_DATA_API_H_
#define EXTENSIONS_API_IMPORT_DATA_IMPORT_DATA_API_H_

#include <memory>
#include <string>
#include <fstream>

#include "base/memory/ref_counted.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/importer/importer_progress_observer.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/importer/importer_type.h"
#include "components/sessions/core/session_id.h"
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
  ProfileSingletonFactory(const ProfileSingletonFactory&) = delete;
  ProfileSingletonFactory& operator=(const ProfileSingletonFactory&) = delete;

 private:
  bool profilesRequested;
  static bool instanceFlag;
  static ProfileSingletonFactory* single;
  std::unique_ptr<ImporterList> api_importer_list_;
  ProfileSingletonFactory();
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

  // Open ifstream to Thunderbird mailbox and set seek position. Leaves fsize
  // unchanged if mailbox is already open.
  bool OpenThunderbirdMailbox(std::string path,
                              std::streampos seek_pos,
                              std::streampos& fsize);

  void CloseThunderbirdMailbox();

  // Read message from Thunderbird mailbox and return seek position after
  // reading. On failure return false and leave seek_pos unchanged.
  bool ReadThunderbirdMessage(std::string& content, std::streampos& seek_pos);

  std::string GetThunderbirdPath() { return thunderbird_mailbox_path_; }

 private:
  friend class BrowserContextKeyedAPIFactory<ImportDataAPI>;

  const raw_ptr<content::BrowserContext> browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "ImportDataAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // If non-null it means importing is in progress. ImporterHost takes care
  // of deleting itself when import is complete.
  raw_ptr<ExternalProcessImporterHost> importer_host_ = nullptr;

  // Keeps count of successful imported items. If not 0 when import ends, one
  // import failed.  Lousy error checking in Chromium forces this.
  int import_succeeded_count_;

  // For reading huge Thunderbird mailbox files.
  std::string thunderbird_mailbox_path_;
  std::ifstream thunderbird_mailbox_;
};

class ImportDataGetProfilesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("importData.getProfiles", IMPORTDATA_GETPROFILES)
  ImportDataGetProfilesFunction();

 private:
  ~ImportDataGetProfilesFunction() override;
  ResponseAction Run() override;

  void Finished();

  raw_ptr<ImporterList> api_importer_list_ = nullptr;
};

class ImportDataStartImportFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("importData.startImport", IMPORTDATA_STARTIMPORT)
  ImportDataStartImportFunction();

 private:
  ~ImportDataStartImportFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void StartImport(const importer::SourceProfile& source_profile);

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  // Items to import
  int imported_items_ = 0;

  // The importer type, need to select correct dialog
  importer::ImporterType importer_type_ = importer::TYPE_UNKNOWN;
};

class ImportDataOpenThunderbirdMailboxFunction : public ExtensionFunction {
  public:
   DECLARE_EXTENSION_FUNCTION("importData.openThunderbirdMailbox",
                              IMPORTDATA_OPENTHUNDERBIRDMAILBOX)
   ImportDataOpenThunderbirdMailboxFunction() = default;

  private:
   ~ImportDataOpenThunderbirdMailboxFunction() override = default;

   ResponseAction Run() override;
};

class ImportDataCloseThunderbirdMailboxFunction : public ExtensionFunction {
  public:
   DECLARE_EXTENSION_FUNCTION("importData.closeThunderbirdMailbox",
                              IMPORTDATA_CLOSETHUNDERBIRDMAILBOX)
   ImportDataCloseThunderbirdMailboxFunction() = default;

 private:
  ~ImportDataCloseThunderbirdMailboxFunction() override = default;

  ResponseAction Run() override;
};

class ImportDataReadMessageFromThunderbirdMailboxFunction
  : public ExtensionFunction {
  public:
   DECLARE_EXTENSION_FUNCTION("importData.readMessageFromThunderbirdMailbox",
                              IMPORTDATA_READMESSAGEFROMTHUNDERBIRMAILBOX)
   ImportDataReadMessageFromThunderbirdMailboxFunction() = default;

  private:
   ~ImportDataReadMessageFromThunderbirdMailboxFunction() override = default;

   ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_IMPORT_DATA_IMPORT_DATA_API_H_
