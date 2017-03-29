// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// See http://www.chromium.org/developers/design-documents/extensions/proposed-changes/creating-new-apis

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMPORT_DATA_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMPORT_DATA_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/importer/importer_progress_observer.h"

#include "content/public/browser/web_ui.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "chrome/browser/shell_integration.h"

class ImporterList;
class ExternalProcessImporterHost;

namespace extensions {

// Singleton that owns all ProfileModels
class ProfileSingletonFactory {
 private:
  bool profilesRequested;
  static bool instanceFlag;
  static ProfileSingletonFactory *single;
  scoped_ptr<ImporterList> api_importer_list_;
  ProfileSingletonFactory();
 public:
  static ProfileSingletonFactory* getInstance();
  ImporterList *getImporterList();
  bool getProfileRequested();
  void setProfileRequested(bool profileReq);
  ~ProfileSingletonFactory();

  DISALLOW_COPY_AND_ASSIGN(ProfileSingletonFactory);
};

class ImporterApiFunction   : public ChromeAsyncExtensionFunction{
 public:
  ImporterApiFunction();
  // AsyncExtensionFunction:
  virtual void SendAsyncResponse();
  ImporterList *api_importer_list;
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
  ImportDataGetProfilesFunction() ;

 protected:
  ~ImportDataGetProfilesFunction() override;
  bool RunAsyncImpl() override;

  DISALLOW_COPY_AND_ASSIGN(ImportDataGetProfilesFunction);
};

class ImportDataStartImportFunction : public ImporterApiFunction,
                                      public importer::ImporterProgressObserver,
                                      public ui::SelectFileDialog::Listener {
 public:
  DECLARE_EXTENSION_FUNCTION("importData.startImport", IMPORTDATA_STARTIMPORT)
  ImportDataStartImportFunction() ;

 protected:
  ~ImportDataStartImportFunction() override;

  struct DialogParams {
    int imported_items;
  };

  // ExtensionFunction:
  bool RunAsyncImpl() override;

  // ui::SelectFileDialog::Listener:
  void FileSelected(const base::FilePath& path,
                            int index,
                            void* params) override;
  void FileSelectionCanceled(void* params) override;

  void StartImport(const importer::SourceProfile& source_profile,
                   uint16 imported_items);
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  void ImportData(const base::ListValue* args);

  void HandleChooseBookmarksFile(const base::ListValue* args);

  // importer::ImporterProgressObserver:
  void ImportStarted() override;
  void ImportItemStarted(importer::ImportItem item) override;
  void ImportItemEnded(importer::ImportItem item) override;
  void ImportEnded() override;

  // If non-null it means importing is in progress. ImporterHost takes care
  // of deleting itself when import is complete.
  ExternalProcessImporterHost* importer_host_;  // weak
  bool import_did_succeed_;

  DISALLOW_COPY_AND_ASSIGN(ImportDataStartImportFunction);
};


class ImportDataSetVivaldiAsDefaultBrowserFunction :
    public ChromeAsyncExtensionFunction ,
    public ShellIntegration::DefaultWebClientObserver {
 public:
  DECLARE_EXTENSION_FUNCTION("importData.setVivaldiAsDefaultBrowser",
                              IMPORTDATA_SETVIVALDIDEFAULT)
  ImportDataSetVivaldiAsDefaultBrowserFunction();

 protected:
  ~ImportDataSetVivaldiAsDefaultBrowserFunction() override;
  scoped_refptr<ShellIntegration::DefaultBrowserWorker> default_browser_worker_;

  // ShellIntegration::DefaultWebClientObserver implementation.
  void SetDefaultWebClientUIState(
    ShellIntegration::DefaultWebClientUIState state) override;
  bool IsInteractiveSetDefaultPermitted() override;

  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(ImportDataSetVivaldiAsDefaultBrowserFunction);
};

class ImportDataIsVivaldiDefaultBrowserFunction :
               public ChromeAsyncExtensionFunction,
               public ShellIntegration::DefaultWebClientObserver {
 public:
  DECLARE_EXTENSION_FUNCTION("importData.isVivaldiDefaultBrowser",
                              IMPORTDATA_ISVIVALDIDEFAULT)
  ImportDataIsVivaldiDefaultBrowserFunction();

 protected:
  ~ImportDataIsVivaldiDefaultBrowserFunction() override;

  scoped_refptr<ShellIntegration::DefaultBrowserWorker> default_browser_worker_;

  // ShellIntegration::DefaultWebClientObserver implementation.
  void SetDefaultWebClientUIState(
     ShellIntegration::DefaultWebClientUIState state) override;
  bool IsInteractiveSetDefaultPermitted() override;

 // ExtensionFunction:
 bool RunAsync() override;

 DISALLOW_COPY_AND_ASSIGN(ImportDataIsVivaldiDefaultBrowserFunction);
};

class ImportDataLaunchNetworkSettingsFunction :
 public ChromeAsyncExtensionFunction{
 public:
  DECLARE_EXTENSION_FUNCTION("importData.launchNetworkSettings",
    IMPORTDATA_LAUNCHNETWORKSETTINGS)
   ImportDataLaunchNetworkSettingsFunction();

 protected:
   ~ImportDataLaunchNetworkSettingsFunction() override;
// ExtensionFunction:
   bool RunAsync() override;

 DISALLOW_COPY_AND_ASSIGN(ImportDataLaunchNetworkSettingsFunction);
};

class ImportDataSavePageFunction :
  public ChromeAsyncExtensionFunction{
 public:
  DECLARE_EXTENSION_FUNCTION("importData.savePage",
  IMPORTDATA_LAUNCHNETWORKSETTINGS)
    ImportDataSavePageFunction();

 protected:
  ~ImportDataSavePageFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(ImportDataSavePageFunction);
};

class ImportDataOpenPageFunction :
  public ChromeAsyncExtensionFunction{
public:
  DECLARE_EXTENSION_FUNCTION("importData.openPage",
  IMPORTDATA_LAUNCHNETWORKSETTINGS)
    ImportDataOpenPageFunction();

protected:
  ~ImportDataOpenPageFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(ImportDataOpenPageFunction);
};


class ImportDataSetVivaldiLanguageFunction :
    public ChromeAsyncExtensionFunction{
  public:
    DECLARE_EXTENSION_FUNCTION("importData.setVivaldiLanguage",
                                IMPORTDATA_SETVIVALDILANGUAGE)
		ImportDataSetVivaldiLanguageFunction();

  protected:
	  ~ImportDataSetVivaldiLanguageFunction() override;
  // ExtensionFunction:
    bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(ImportDataSetVivaldiLanguageFunction);
};

class ImportDataSetDefaultContentSettingsFunction :
  public ChromeAsyncExtensionFunction{
 public:
   DECLARE_EXTENSION_FUNCTION("importData.setDefaultContentSettings",
   IMPORTDATA_SETDEFAULTCONTENTSETTING)
   ImportDataSetDefaultContentSettingsFunction();

 protected:
  ~ImportDataSetDefaultContentSettingsFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(ImportDataSetDefaultContentSettingsFunction);
};

class ImportDataGetDefaultContentSettingsFunction :
	public ChromeAsyncExtensionFunction{
public:
	DECLARE_EXTENSION_FUNCTION("importData.getDefaultContentSettings",
	                            IMPORTDATA_SETDEFAULTCONTENTSETTING)
		ImportDataGetDefaultContentSettingsFunction();

protected:
	~ImportDataGetDefaultContentSettingsFunction() override;
	// ExtensionFunction:
	bool RunAsync() override;

	DISALLOW_COPY_AND_ASSIGN(ImportDataGetDefaultContentSettingsFunction);
};


class ImportDataSetBlockThirdPartyCookiesFunction :
	public ChromeAsyncExtensionFunction{
public:
	DECLARE_EXTENSION_FUNCTION("importData.setBlockThirdPartyCookies",
	                          IMPORTDATA_SET_BLOCKTHIRDPARTYCOOKIES)
		ImportDataSetBlockThirdPartyCookiesFunction();

protected:
	~ImportDataSetBlockThirdPartyCookiesFunction()override;
	// ExtensionFunction:
	bool RunAsync() override;

	DISALLOW_COPY_AND_ASSIGN(ImportDataSetBlockThirdPartyCookiesFunction);
};

class ImportDataGetBlockThirdPartyCookiesFunction :
	public ChromeAsyncExtensionFunction{
public:
	DECLARE_EXTENSION_FUNCTION("importData.getBlockThirdPartyCookies",
	                            IMPORTDATA_GET_BLOCKTHIRDPARTYCOOKIES)
		ImportDataGetBlockThirdPartyCookiesFunction();

protected:
	~ImportDataGetBlockThirdPartyCookiesFunction() override;
	// ExtensionFunction:
	bool RunAsync() override;

	DISALLOW_COPY_AND_ASSIGN(ImportDataGetBlockThirdPartyCookiesFunction);
};


class ImportDataOpenTaskManagerFunction :
  public ChromeAsyncExtensionFunction{
 public:
  DECLARE_EXTENSION_FUNCTION("importData.openTaskManager",
  IMPORTDATA_GET_BLOCKTHIRDPARTYCOOKIES)
    ImportDataOpenTaskManagerFunction();

 protected:
   ~ImportDataOpenTaskManagerFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(ImportDataOpenTaskManagerFunction);
};

class ImportDataShowDevToolsFunction :
  public ChromeAsyncExtensionFunction{
 public:
  DECLARE_EXTENSION_FUNCTION("importData.showDevTools",
  IMPORTDATA_GET_BLOCKTHIRDPARTYCOOKIES)
    ImportDataShowDevToolsFunction();

 protected:
   ~ImportDataShowDevToolsFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(ImportDataShowDevToolsFunction);
};


class ImportDataGetStartupActionFunction :
  public ChromeAsyncExtensionFunction{
 public:
  DECLARE_EXTENSION_FUNCTION("importData.getStartupAction",
  IMPORTDATA_GET_STARTUPTYPE)
    ImportDataGetStartupActionFunction();

 protected:
  ~ImportDataGetStartupActionFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(ImportDataGetStartupActionFunction);
};


class ImportDataSetStartupActionFunction :
  public ChromeAsyncExtensionFunction{
 public:
  DECLARE_EXTENSION_FUNCTION("importData.setStartupAction",
  IMPORTDATA_SET_STARTUPTYPE)
    ImportDataSetStartupActionFunction();

 protected:
   ~ImportDataSetStartupActionFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(ImportDataSetStartupActionFunction);
};


}

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMPORT_DATA_API_H_
