//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#ifndef EXTENSIONS_API_VIVALDI_UTILITIES_VIVALDI_UTILITIES_API_H_
#define EXTENSIONS_API_VIVALDI_UTILITIES_VIVALDI_UTILITIES_API_H_

#include <map>
#include <memory>
#include <string>

#include "base/lazy_instance.h"
#include "base/power_monitor/power_observer.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/password_manager/reauth_purpose.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/passwords/settings/password_access_authenticator.h"
#include "content/public/browser/download_manager.h"
#include "extensions/browser/api/file_system/file_system_api.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/schema/vivaldi_utilities.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/lights/razer_chroma_handler.h"

class Browser;

namespace extensions {

class VivaldiUtilitiesAPI : public BrowserContextKeyedAPI,
                            public EventRouter::Observer,
                            public base::PowerObserver,
                            public content::DownloadManager::Observer {
 public:
  explicit VivaldiUtilitiesAPI(content::BrowserContext* context);
  ~VivaldiUtilitiesAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<VivaldiUtilitiesAPI>*
  GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

  // Fires an event to JS with the active device that triggers scrolling.
  // 1: Mouse, 2: Trackpad 3: Inertial"
  static void ScrollType(content::BrowserContext* browser_context,
                         int scrollType);

  // Returns true if the key didn't not exist previously, false if it updated
  // an existing value
  bool SetSharedData(const std::string& key, base::Value* value);

  // Looks up an existing key/value pair, returns nullptr if the key does not
  // exist.
  const base::Value* GetSharedData(const std::string& key);

  // Sets anchor rect for the named dialog. Returns true if the key didn't not
  // exist previously, false if it updated an existing value
  bool SetDialogPostion(int window_id,
                        const std::string& dialog,
                        const gfx::Rect& rect,
                        const std::string& flow_direction);

  // Sets anchor rect for the named dialog
  gfx::Rect GetDialogPosition(int window_id,
                              const std::string& dialog,
                              std::string* flow_direction);

  // PowerObserver implementation
  void OnPowerStateChange(bool on_battery_power) override;
  void OnSuspend() override;
  void OnResume() override;

  // DownloadManager::Observer implementation
  void OnManagerInitialized() override;

  void OnPasswordIconStatusChanged(int window_id, bool show);

  // Close all app windows generating thumbnails.
  void CloseAllThumbnailWindows();

  // Trigger the OS authentication dialog, if needed.
  bool AuthenticateUser(gfx::NativeWindow window);

  // Is the Razer Chroma API available on this machine
  bool IsRazerChromaAvailable();

  // Is the Razer Chroma API ready to accept commands.
  bool IsRazerChromaReady();

  // Set RGB color of the configured Razer Chroma devices.
  bool SetRazerChromaColors(RazerChromaColors& colors);

  class DialogPosition {
   public:
    DialogPosition(int window_id,
                   const std::string& dialog_name,
                   gfx::Rect rect,
                   const std::string& flow_direction);

    ~DialogPosition() {}

    int window_id() { return window_id_; }
    const std::string& dialog_name() { return dialog_name_; }
    const gfx::Rect& rect() { return rect_; }
    const std::string& flow_direction() { return flow_direction_;  }

   private:
    int window_id_ = 0;
    std::string dialog_name_;
    gfx::Rect rect_;
    std::string flow_direction_;
  };
 protected:
  bool OsReauthCall(password_manager::ReauthPurpose purpose);

 private:
  friend class BrowserContextKeyedAPIFactory<VivaldiUtilitiesAPI>;

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "UtilitiesAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Map used for the *sharedData apis.
  std::map<std::string, base::Value*> key_to_values_map_;

  // List used for the dialog position apis.
  std::vector<std::unique_ptr<DialogPosition> > dialog_to_point_list_;

  // Persistent class used for re-authentication of the user when viewing
  // saved passwords. It cannot be instanciated per call as it keeps state
  // of previous authentiations.
  PasswordAccessAuthenticator password_access_authenticator_;

  // Used to anchor the auth dialog.
  gfx::NativeWindow native_window_ = nullptr;

  // Razer Chroma integration, if available.
  std::unique_ptr<RazerChromaHandler> razer_chroma_handler_;
};

class UtilitiesPrintFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.print", UTILITIES_PRINT)
  UtilitiesPrintFunction() = default;

  ResponseAction Run() override;

 protected:
  ~UtilitiesPrintFunction() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesPrintFunction);
};

class UtilitiesClearAllRecentlyClosedSessionsFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.clearAllRecentlyClosedSessions",
                             UTILITIES_CLEARALLRECENTLYCLOSEDSESSIONS)
  UtilitiesClearAllRecentlyClosedSessionsFunction() = default;

 protected:
  ~UtilitiesClearAllRecentlyClosedSessionsFunction() override = default;

  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesClearAllRecentlyClosedSessionsFunction);
};

// Obtains a 64-bit integer, encoded as 16 hex characters to be used by piwik
// as a unique user id. The user id is stored in the vivaldi user profile
// with a backup copy stored in the OS user profile (registry on Windows).
// If no stored user id is found, a new one is generated.
class UtilitiesGetUniqueUserIdFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getUniqueUserId",
                             UTILITIES_GETUNIQUEUSERID)
  UtilitiesGetUniqueUserIdFunction() = default;

 protected:
  ~UtilitiesGetUniqueUserIdFunction() override = default;

  ResponseAction Run() override;

  bool ReadUserIdFromOSProfile(std::string* user_id);
  void WriteUserIdToOSProfile(const std::string& user_id);

  void GetUniqueUserIdTask(const std::string& legacy_user_id);
  void RespondOnUIThread(const std::string& user_id, bool is_new_user);

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesGetUniqueUserIdFunction);
};

class UtilitiesIsTabInLastSessionFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.isTabInLastSession",
                             UTILITIES_ISTABINLASTSESSION)
  UtilitiesIsTabInLastSessionFunction() = default;

 protected:
  ~UtilitiesIsTabInLastSessionFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesIsTabInLastSessionFunction);
};

class UtilitiesIsUrlValidFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.isUrlValid", UTILITIES_ISURLVALID)
  UtilitiesIsUrlValidFunction();

 protected:
  ~UtilitiesIsUrlValidFunction() override;

  void OnDefaultProtocolClientWorkerFinished(
      shell_integration::DefaultWebClientState state);

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  bool prompt_user_ = false;
  vivaldi::utilities::UrlValidResults result_;

  DISALLOW_COPY_AND_ASSIGN(UtilitiesIsUrlValidFunction);
};

class UtilitiesGetSelectedTextFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getSelectedText",
                             UTILITIES_GETSELECTEDTEXT)
  UtilitiesGetSelectedTextFunction() = default;

 protected:
  ~UtilitiesGetSelectedTextFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesGetSelectedTextFunction);
};

class UtilitiesCreateUrlMappingFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.createUrlMapping",
                             UTILITIES_CREATEURLMAPPING)
  UtilitiesCreateUrlMappingFunction() = default;

 protected:
  ~UtilitiesCreateUrlMappingFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesCreateUrlMappingFunction);
};

class UtilitiesRemoveUrlMappingFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.removeUrlMapping",
                             UTILITIES_REMOVEURLMAPPING)
  UtilitiesRemoveUrlMappingFunction() = default;

 protected:
  ~UtilitiesRemoveUrlMappingFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesRemoveUrlMappingFunction);
};

class UtilitiesSelectFileFunction : public UIThreadExtensionFunction,
                                    public ui::SelectFileDialog::Listener {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.selectFile", UTILITIES_SELECTFILE)
  UtilitiesSelectFileFunction();

 protected:
  ~UtilitiesSelectFileFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  // ui::SelectFileDialog::Listener:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectionCanceled(void* params) override;

 private:
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  DISALLOW_COPY_AND_ASSIGN(UtilitiesSelectFileFunction);
};

class UtilitiesGetVersionFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getVersion",
                             UTILITIES_GETVERSION)
  UtilitiesGetVersionFunction() = default;

 protected:
  ~UtilitiesGetVersionFunction() override = default;

  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesGetVersionFunction);
};

class UtilitiesSetSharedDataFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.setSharedData", UTILITIES_SETSHAREDDATA)

  UtilitiesSetSharedDataFunction() = default;

 protected:
  ~UtilitiesSetSharedDataFunction() override = default;

  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesSetSharedDataFunction);
};

class UtilitiesGetSharedDataFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getSharedData", UTILITIES_GETSHAREDDATA)

  UtilitiesGetSharedDataFunction() = default;

 protected:
  ~UtilitiesGetSharedDataFunction() override = default;

  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesGetSharedDataFunction);
};

class UtilitiesGetSystemDateFormatFunction : public UIThreadExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("utilities.getSystemDateFormat",
                             UTILITIES_GETSYSTEM_DATE_FORMAT)
 public:
  UtilitiesGetSystemDateFormatFunction() = default;

  bool ReadDateFormats(vivaldi::utilities::DateFormats* date_formats);

 protected:
  ~UtilitiesGetSystemDateFormatFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesGetSystemDateFormatFunction);
};

class UtilitiesSetLanguageFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.setLanguage", UTILITIES_SETLANGUAGE)
  UtilitiesSetLanguageFunction() = default;

 protected:
  ~UtilitiesSetLanguageFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesSetLanguageFunction);
};

class UtilitiesGetLanguageFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getLanguage", UTILITIES_GETLANGUAGE)
  UtilitiesGetLanguageFunction() = default;

 protected:
  ~UtilitiesGetLanguageFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesGetLanguageFunction);
};

class UtilitiesSetVivaldiAsDefaultBrowserFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.setVivaldiAsDefaultBrowser",
                             UTILITIES_SETVIVALDIDEFAULT)
  UtilitiesSetVivaldiAsDefaultBrowserFunction() = default;

 protected:
  ~UtilitiesSetVivaldiAsDefaultBrowserFunction() override;

  void OnDefaultBrowserWorkerFinished(
      shell_integration::DefaultWebClientState state);

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesSetVivaldiAsDefaultBrowserFunction);
};

class UtilitiesIsVivaldiDefaultBrowserFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.isVivaldiDefaultBrowser",
                             UTILITIES_ISVIVALDIDEFAULT)
  UtilitiesIsVivaldiDefaultBrowserFunction() = default;

 protected:
  ~UtilitiesIsVivaldiDefaultBrowserFunction() override;

  void OnDefaultBrowserWorkerFinished(
      shell_integration::DefaultWebClientState state);

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesIsVivaldiDefaultBrowserFunction);
};

class UtilitiesLaunchNetworkSettingsFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.launchNetworkSettings",
                             UTILITIES_LAUNCHNETWORKSETTINGS)
  UtilitiesLaunchNetworkSettingsFunction() = default;

 protected:
  ~UtilitiesLaunchNetworkSettingsFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesLaunchNetworkSettingsFunction);
};

class UtilitiesSavePageFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.savePage", UTILITIES_SAVEPAGE)
  UtilitiesSavePageFunction() = default;

 protected:
  ~UtilitiesSavePageFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesSavePageFunction);
};

class UtilitiesOpenPageFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.openPage",
                             UTILITIES_OPENPAGE)
  UtilitiesOpenPageFunction() = default;

 protected:
  ~UtilitiesOpenPageFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesOpenPageFunction);
};

class UtilitiesSetDefaultContentSettingsFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.setDefaultContentSettings",
                             UTILITIES_SETDEFAULTCONTENTSETTING)
  UtilitiesSetDefaultContentSettingsFunction() = default;

 protected:
  ~UtilitiesSetDefaultContentSettingsFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesSetDefaultContentSettingsFunction);
};

class UtilitiesGetDefaultContentSettingsFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getDefaultContentSettings",
                             UTILITIES_GETDEFAULTCONTENTSETTING)
  UtilitiesGetDefaultContentSettingsFunction() = default;

 protected:
  ~UtilitiesGetDefaultContentSettingsFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesGetDefaultContentSettingsFunction);
};

class UtilitiesSetBlockThirdPartyCookiesFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.setBlockThirdPartyCookies",
                             UTILITIES_SET_BLOCKTHIRDPARTYCOOKIES)
  UtilitiesSetBlockThirdPartyCookiesFunction() = default;

 protected:
  ~UtilitiesSetBlockThirdPartyCookiesFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesSetBlockThirdPartyCookiesFunction);
};

class UtilitiesGetBlockThirdPartyCookiesFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getBlockThirdPartyCookies",
                             UTILITIES_GET_BLOCKTHIRDPARTYCOOKIES)
  UtilitiesGetBlockThirdPartyCookiesFunction() = default;

 protected:
  ~UtilitiesGetBlockThirdPartyCookiesFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesGetBlockThirdPartyCookiesFunction);
};

class UtilitiesOpenTaskManagerFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.openTaskManager",
                             UTILITIES_OPENTASKMANAGER)
  UtilitiesOpenTaskManagerFunction() = default;

 protected:
  ~UtilitiesOpenTaskManagerFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesOpenTaskManagerFunction);
};


class UtilitiesGetStartupActionFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getStartupAction",
                             UTILITIES_GET_STARTUPTYPE)
  UtilitiesGetStartupActionFunction() = default;

 protected:
  ~UtilitiesGetStartupActionFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesGetStartupActionFunction);
};

class UtilitiesSetStartupActionFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.setStartupAction",
                             UTILITIES_SET_STARTUPTYPE)
  UtilitiesSetStartupActionFunction() = default;

 protected:
  ~UtilitiesSetStartupActionFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesSetStartupActionFunction);
};

class UtilitiesCanShowWelcomePageFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.canShowWelcomePage",
                             UTILITIES_CANSHOWWELCOMEPAGE)
  UtilitiesCanShowWelcomePageFunction() = default;

  ResponseAction Run() override;

 protected:
  ~UtilitiesCanShowWelcomePageFunction() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesCanShowWelcomePageFunction);
};

class UtilitiesCanShowWhatsNewPageFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.canShowWhatsNewPage",
                             UTILITIES_CANSHOWWHATSNEWPAGE)
  UtilitiesCanShowWhatsNewPageFunction() = default;

  ResponseAction Run() override;

 protected:
  ~UtilitiesCanShowWhatsNewPageFunction() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesCanShowWhatsNewPageFunction);
};

class UtilitiesShowPasswordDialogFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.showPasswordDialog",
                             UTILITIES_SHOW_PASSWORD_DIALOG)
  UtilitiesShowPasswordDialogFunction() = default;

 private:
  ~UtilitiesShowPasswordDialogFunction() override = default;
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(UtilitiesShowPasswordDialogFunction);
};

class UtilitiesSetDialogPositionFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.setDialogPosition",
                             UTILITIES_SET_DIALOG_POSITION)
  UtilitiesSetDialogPositionFunction() = default;

 private:
  ~UtilitiesSetDialogPositionFunction() override = default;
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(UtilitiesSetDialogPositionFunction);
};

class UtilitiesIsRazerChromaAvailableFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.isRazerChromaAvailable",
                             UTILITIES_IS_RAZER_CHROMA_AVAILABLE)
  UtilitiesIsRazerChromaAvailableFunction() = default;

 private:
  ~UtilitiesIsRazerChromaAvailableFunction() override = default;
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(UtilitiesIsRazerChromaAvailableFunction);
};

class UtilitiesIsRazerChromaReadyFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.isRazerChromaReady",
                             UTILITIES_IS_RAZER_CHROMA_READY)
  UtilitiesIsRazerChromaReadyFunction() = default;

 private:
  ~UtilitiesIsRazerChromaReadyFunction() override = default;
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(UtilitiesIsRazerChromaReadyFunction);
};

class UtilitiesSetRazerChromaColorFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.setRazerChromaColor",
                             UTILITIES_SET_RAZER_CHROMA_COLOR)
  UtilitiesSetRazerChromaColorFunction() = default;

 private:
  ~UtilitiesSetRazerChromaColorFunction() override = default;
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(UtilitiesSetRazerChromaColorFunction);
};

class UtilitiesIsDownloadManagerReadyFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.isDownloadManagerReady",
                             UTILITIES_IS_DOWNLOAD_MANAGER_READY)
  UtilitiesIsDownloadManagerReadyFunction() = default;

 private:
  ~UtilitiesIsDownloadManagerReadyFunction() override = default;
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(UtilitiesIsDownloadManagerReadyFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_VIVALDI_UTILITIES_VIVALDI_UTILITIES_API_H_
