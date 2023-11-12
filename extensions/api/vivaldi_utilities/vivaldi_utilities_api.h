//
// Copyright (c) 2015-2022 Vivaldi Technologies AS. All rights reserved.
//

#ifndef EXTENSIONS_API_VIVALDI_UTILITIES_VIVALDI_UTILITIES_API_H_
#define EXTENSIONS_API_VIVALDI_UTILITIES_VIVALDI_UTILITIES_API_H_

#include <map>
#include <memory>
#include <string>

#include "base/containers/queue.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted_memory.h"
#include "base/power_monitor/power_observer.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/services/qrcode_generator/public/cpp/qrcode_generator_service.h"
#include "chrome/services/qrcode_generator/public/mojom/qrcode_generator.mojom.h"
#include "components/password_manager/core/browser/password_access_authenticator.h"
#include "components/password_manager/core/browser/reauth_purpose.h"
#include "content/public/browser/download_manager.h"
#include "extensions/browser/api/file_system/file_system_api.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#include "browser/translate/vivaldi_translate_server_request.h"
#include "components/datasource/vivaldi_image_store.h"
#include "extensions/schema/vivaldi_utilities.h"
#include "ui/lights/razer_chroma_handler.h"

class Browser;

namespace extensions {

class VivaldiUtilitiesAPI : public BrowserContextKeyedAPI,
                            public EventRouter::Observer,
                            public base::PowerStateObserver,
                            public base::PowerSuspendObserver,
                            public content::DownloadManager::Observer {
 public:
  using MutexAvailableCallback = base::OnceCallback<void(int)>;

  explicit VivaldiUtilitiesAPI(content::BrowserContext* context);
  ~VivaldiUtilitiesAPI() override;

  void PostProfileSetup();

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
  bool SetSharedData(const std::string& key, base::Value value);

  // Looks up an existing key/value pair, returns nullptr if the key does not
  // exist.
  const base::Value* GetSharedData(const std::string& key);

  bool TakeMutex(const std::string& name,
                 bool wait,
                 MutexAvailableCallback callback);
  bool ReleaseMutex(const std::string& name, int release_token);

  // Sets anchor rect for the named dialog. Returns true if the key didn't not
  // exist previously, false if it updated an existing value
  bool SetDialogPostion(int window_id,
                        const std::string& dialog,
                        const gfx::Rect& rect,
                        const std::string& flow_direction);

  // Gets anchor rect for the named dialog
  gfx::Rect GetDialogPosition(int window_id,
                              const std::string& dialog,
                              std::string* flow_direction);

  // PowerObserver implementation
  void OnPowerStateChange(bool on_battery_power) override;
  void OnSuspend() override;
  void OnResume() override;

  // DownloadManager::Observer implementation
  void OnManagerInitialized() override;
  void ManagerGoingDown(content::DownloadManager* manager) override;

  void OnPasswordIconStatusChanged(int window_id, bool show);

  // Trigger the OS authentication dialog, if needed. web_contents must not be
  // null.
  static bool AuthenticateUser(
      content::WebContents* web_contents,
      password_manager::PasswordAccessAuthenticator::AuthResultCallback
          callback);

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
    const std::string& flow_direction() { return flow_direction_; }

   private:
    int window_id_ = 0;
    std::string dialog_name_;
    gfx::Rect rect_;
    std::string flow_direction_;
  };

 private:
  friend class BrowserContextKeyedAPIFactory<VivaldiUtilitiesAPI>;

  struct MutexData {
    explicit MutexData(int release_token);
    ~MutexData();

    MutexData(MutexData&&);
    MutexData& operator=(MutexData&&);

    int release_token;
    base::queue<std::pair<int, MutexAvailableCallback>> wait_list;
  };

  void OsReauthCall(
      password_manager::ReauthPurpose purpose,
      password_manager::PasswordAccessAuthenticator::AuthResultCallback
          callback);

  void TimeoutCall();

  const raw_ptr<content::BrowserContext> browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "UtilitiesAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Map used for the *sharedData apis.
  std::map<std::string, base::Value> key_to_values_map_;

  std::map<std::string, MutexData> mutexes_;

  // List used for the dialog position apis.
  std::vector<std::unique_ptr<DialogPosition>> dialog_to_point_list_;

  // Persistent class used for re-authentication of the user when viewing
  // saved passwords. It cannot be instanciated per call as it keeps state
  // of previous authentiations.
  password_manager::PasswordAccessAuthenticator password_access_authenticator_;

  // Used to anchor the auth dialog.
  gfx::NativeWindow native_window_ = nullptr;

  // Razer Chroma integration, if available.
  std::unique_ptr<RazerChromaHandler> razer_chroma_handler_;
};

class UtilitiesPrintFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.print", UTILITIES_PRINT)
  UtilitiesPrintFunction() = default;

  ResponseAction Run() override;

 private:
  ~UtilitiesPrintFunction() override = default;
};

class UtilitiesClearAllRecentlyClosedSessionsFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.clearAllRecentlyClosedSessions",
                             UTILITIES_CLEARALLRECENTLYCLOSEDSESSIONS)
  UtilitiesClearAllRecentlyClosedSessionsFunction() = default;

 private:
  ~UtilitiesClearAllRecentlyClosedSessionsFunction() override = default;

  ResponseAction Run() override;
};

class UtilitiesIsTabInLastSessionFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.isTabInLastSession",
                             UTILITIES_ISTABINLASTSESSION)
  UtilitiesIsTabInLastSessionFunction() = default;

 private:
  ~UtilitiesIsTabInLastSessionFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

// This is implemented in VivaldiUtilitiesHookDelegate and only here to satisfy
// various JS bindings constrains.
class UtilitiesIsUrlValidFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.isUrlValid", UTILITIES_ISURLVALID)
  UtilitiesIsUrlValidFunction() = default;

 private:
  ~UtilitiesIsUrlValidFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class UtilitiesCanOpenUrlExternallyFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.canOpenUrlExternally",
                             UTILITIES_CANOPENURLEXTERNALLY)
  UtilitiesCanOpenUrlExternallyFunction() = default;

 private:
  ~UtilitiesCanOpenUrlExternallyFunction() override = default;

  void OnDefaultProtocolClientWorkerFinished(
      shell_integration::DefaultWebClientState state);

  // ExtensionFunction:
  ResponseAction Run() override;
};

// This is implemented in VivaldiUtilitiesHookDelegate and only here to satisfy
// various JS bindings constrains.
class UtilitiesGetUrlFragmentsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getUrlFragments",
                             UTILITIES_GET_URL_FRAGMENTS)
  UtilitiesGetUrlFragmentsFunction() = default;

 private:
  ~UtilitiesGetUrlFragmentsFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

// This is implemented in VivaldiUtilitiesHookDelegate and only here to satisfy
// various JS bindings constrains.
class UtilitiesUrlToThumbnailTextFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.urlToThumbnailText",
                             UTILITIES_URLTOTHUMBNAILTEXT)
  UtilitiesUrlToThumbnailTextFunction() = default;

 private:
  ~UtilitiesUrlToThumbnailTextFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class UtilitiesGetSelectedTextFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getSelectedText",
                             UTILITIES_GETSELECTEDTEXT)
  UtilitiesGetSelectedTextFunction() = default;

 private:
  ~UtilitiesGetSelectedTextFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class UtilitiesSelectFileFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.selectFile", UTILITIES_SELECTFILE)
  UtilitiesSelectFileFunction() = default;

 private:
  ~UtilitiesSelectFileFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnFileSelected(base::FilePath path, bool cancelled);
};

class UtilitiesSelectLocalImageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.selectLocalImage",
                             UTILITIES_SELECTLOCALIMAGE)
  UtilitiesSelectLocalImageFunction() = default;

 private:
  ~UtilitiesSelectLocalImageFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnFileSelected(VivaldiImageStore::ImagePlace place,
                      bool store_as_profile_image,
                      base::FilePath path,
                      bool cancelled);

  void OnContentRead(int64_t bookmark_id,
                     absl::optional<std::vector<uint8_t>> content);

  void SendResult(std::string data_url);
};

class UtilitiesStoreImageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.storeImage", UTILITIES_STOREIMAGE)
  UtilitiesStoreImageFunction();

 private:
  ~UtilitiesStoreImageFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void StoreImage(scoped_refptr<base::RefCountedMemory> data);
  void SendResult(std::string data_url);

  VivaldiImageStore::ImagePlace place_;
  absl::optional<VivaldiImageStore::ImageFormat> image_format_;
};

// This is implemented in VivaldiUtilitiesHookDelegate and only here to satisfy
// various JS bindings constrains.
class UtilitiesGetVersionFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getVersion", UTILITIES_GETVERSION)
  UtilitiesGetVersionFunction() = default;

 private:
  ~UtilitiesGetVersionFunction() override = default;

  ResponseAction Run() override;
};

class UtilitiesGetFFMPEGStateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getFFMPEGState",
                             UTILITIES_GET_FFMPEG_STATE)
  UtilitiesGetFFMPEGStateFunction() = default;

 private:
  ~UtilitiesGetFFMPEGStateFunction() override = default;

  ResponseAction Run() override;
};

class UtilitiesGetMediaAvailableStateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getMediaAvailableState",
                             UTILITIES_GET_MEDIA_AVAILABLE_STATE)
  UtilitiesGetMediaAvailableStateFunction() = default;

 private:
  ~UtilitiesGetMediaAvailableStateFunction() override = default;

  ResponseAction Run() override;
};

class UtilitiesSetSharedDataFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.setSharedData", UTILITIES_SETSHAREDDATA)

  UtilitiesSetSharedDataFunction() = default;

 private:
  ~UtilitiesSetSharedDataFunction() override = default;

  ResponseAction Run() override;
};

class UtilitiesGetSharedDataFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getSharedData", UTILITIES_GETSHAREDDATA)

  UtilitiesGetSharedDataFunction() = default;

 private:
  ~UtilitiesGetSharedDataFunction() override = default;

  ResponseAction Run() override;
};

class UtilitiesTakeMutexFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.takeMutex", UTILITIES_TAKEMUTEX)

  UtilitiesTakeMutexFunction() = default;

 private:
  ~UtilitiesTakeMutexFunction() override = default;

  ResponseAction Run() override;
  void OnMutexAcquired(std::string name, int release_token);
};

class UtilitiesReleaseMutexFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.releaseMutex", UTILITIES_RELEASEMUTEX)

  UtilitiesReleaseMutexFunction() = default;

 private:
  ~UtilitiesReleaseMutexFunction() override = default;

  ResponseAction Run() override;
};

class UtilitiesGetSystemDateFormatFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("utilities.getSystemDateFormat",
                             UTILITIES_GETSYSTEM_DATE_FORMAT)
 public:
  UtilitiesGetSystemDateFormatFunction() = default;

 private:
  ~UtilitiesGetSystemDateFormatFunction() override = default;

  bool ReadDateFormats(vivaldi::utilities::DateFormats* date_formats);

  // ExtensionFunction:
  ResponseAction Run() override;
};

class UtilitiesGetSystemCountryFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getSystemCountry",
                             UTILITIES_GET_SYSTEM_COUNTRY)
  UtilitiesGetSystemCountryFunction() = default;

 private:
  ~UtilitiesGetSystemCountryFunction() override = default;

  ResponseAction Run() override;
};

class UtilitiesSetLanguageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.setLanguage", UTILITIES_SETLANGUAGE)
  UtilitiesSetLanguageFunction() = default;

 private:
  ~UtilitiesSetLanguageFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;
};

class UtilitiesGetLanguageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getLanguage", UTILITIES_GETLANGUAGE)
  UtilitiesGetLanguageFunction() = default;

 private:
  ~UtilitiesGetLanguageFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;
};

class UtilitiesSetVivaldiAsDefaultBrowserFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.setVivaldiAsDefaultBrowser",
                             UTILITIES_SETVIVALDIDEFAULT)
  UtilitiesSetVivaldiAsDefaultBrowserFunction() = default;

 private:
  ~UtilitiesSetVivaldiAsDefaultBrowserFunction() override;

  void OnDefaultBrowserWorkerFinished(
      shell_integration::DefaultWebClientState state);

  // ExtensionFunction:
  ResponseAction Run() override;
};

class UtilitiesIsVivaldiDefaultBrowserFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.isVivaldiDefaultBrowser",
                             UTILITIES_ISVIVALDIDEFAULT)
  UtilitiesIsVivaldiDefaultBrowserFunction() = default;

 private:
  ~UtilitiesIsVivaldiDefaultBrowserFunction() override;

  void OnDefaultBrowserWorkerFinished(
      shell_integration::DefaultWebClientState state);

  // ExtensionFunction:
  ResponseAction Run() override;
};

class UtilitiesLaunchNetworkSettingsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.launchNetworkSettings",
                             UTILITIES_LAUNCHNETWORKSETTINGS)
  UtilitiesLaunchNetworkSettingsFunction() = default;

 private:
  ~UtilitiesLaunchNetworkSettingsFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;
};

class UtilitiesSavePageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.savePage", UTILITIES_SAVEPAGE)
  UtilitiesSavePageFunction() = default;

 private:
  ~UtilitiesSavePageFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;
};

class UtilitiesOpenPageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.openPage", UTILITIES_OPENPAGE)
  UtilitiesOpenPageFunction() = default;

 private:
  ~UtilitiesOpenPageFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;
};

class UtilitiesBroadcastMessageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.broadcastMessage",
                             UTILITIES_BROADCAST_MESSAGE)
  UtilitiesBroadcastMessageFunction() = default;

 private:
  ~UtilitiesBroadcastMessageFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;
};

class UtilitiesSetDefaultContentSettingsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.setDefaultContentSettings",
                             UTILITIES_SETDEFAULTCONTENTSETTING)
  UtilitiesSetDefaultContentSettingsFunction() = default;

 private:
  ~UtilitiesSetDefaultContentSettingsFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;
};

class UtilitiesGetDefaultContentSettingsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getDefaultContentSettings",
                             UTILITIES_GETDEFAULTCONTENTSETTING)
  UtilitiesGetDefaultContentSettingsFunction() = default;

 private:
  ~UtilitiesGetDefaultContentSettingsFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;
};

class UtilitiesSetBlockThirdPartyCookiesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.setBlockThirdPartyCookies",
                             UTILITIES_SET_BLOCKTHIRDPARTYCOOKIES)
  UtilitiesSetBlockThirdPartyCookiesFunction() = default;

 private:
  ~UtilitiesSetBlockThirdPartyCookiesFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;
};

class UtilitiesGetBlockThirdPartyCookiesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getBlockThirdPartyCookies",
                             UTILITIES_GET_BLOCKTHIRDPARTYCOOKIES)
  UtilitiesGetBlockThirdPartyCookiesFunction() = default;

 private:
  ~UtilitiesGetBlockThirdPartyCookiesFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;
};

class UtilitiesOpenTaskManagerFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.openTaskManager",
                             UTILITIES_OPENTASKMANAGER)
  UtilitiesOpenTaskManagerFunction() = default;

 private:
  ~UtilitiesOpenTaskManagerFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;
};

class UtilitiesCreateQRCodeFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.createQRCode", UTILITIES_CREATE_QR_CODE)
  UtilitiesCreateQRCodeFunction() = default;

 private:
  ~UtilitiesCreateQRCodeFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;
};

class UtilitiesGetStartupActionFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getStartupAction",
                             UTILITIES_GET_STARTUPTYPE)
  UtilitiesGetStartupActionFunction() = default;

 private:
  ~UtilitiesGetStartupActionFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;
};

class UtilitiesSetStartupActionFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.setStartupAction",
                             UTILITIES_SET_STARTUPTYPE)
  UtilitiesSetStartupActionFunction() = default;

 private:
  ~UtilitiesSetStartupActionFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;
};

class UtilitiesCanShowWhatsNewPageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.canShowWhatsNewPage",
                             UTILITIES_CANSHOWWHATSNEWPAGE)
  UtilitiesCanShowWhatsNewPageFunction() = default;

  ResponseAction Run() override;

 private:
  ~UtilitiesCanShowWhatsNewPageFunction() override = default;
};

class UtilitiesShowPasswordDialogFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.showPasswordDialog",
                             UTILITIES_SHOW_PASSWORD_DIALOG)
  UtilitiesShowPasswordDialogFunction() = default;

 private:
  ~UtilitiesShowPasswordDialogFunction() override = default;
  ResponseAction Run() override;
};

class UtilitiesSetDialogPositionFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.setDialogPosition",
                             UTILITIES_SET_DIALOG_POSITION)
  UtilitiesSetDialogPositionFunction() = default;

 private:
  ~UtilitiesSetDialogPositionFunction() override = default;
  ResponseAction Run() override;
};

class UtilitiesIsRazerChromaAvailableFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.isRazerChromaAvailable",
                             UTILITIES_IS_RAZER_CHROMA_AVAILABLE)
  UtilitiesIsRazerChromaAvailableFunction() = default;

 private:
  ~UtilitiesIsRazerChromaAvailableFunction() override = default;
  ResponseAction Run() override;
};

class UtilitiesIsRazerChromaReadyFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.isRazerChromaReady",
                             UTILITIES_IS_RAZER_CHROMA_READY)
  UtilitiesIsRazerChromaReadyFunction() = default;

 private:
  ~UtilitiesIsRazerChromaReadyFunction() override = default;
  ResponseAction Run() override;
};

class UtilitiesSetRazerChromaColorFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.setRazerChromaColor",
                             UTILITIES_SET_RAZER_CHROMA_COLOR)
  UtilitiesSetRazerChromaColorFunction() = default;

 private:
  ~UtilitiesSetRazerChromaColorFunction() override = default;
  ResponseAction Run() override;
};

class UtilitiesIsDownloadManagerReadyFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.isDownloadManagerReady",
                             UTILITIES_IS_DOWNLOAD_MANAGER_READY)
  UtilitiesIsDownloadManagerReadyFunction() = default;

 private:
  ~UtilitiesIsDownloadManagerReadyFunction() override = default;
  ResponseAction Run() override;
};

class UtilitiesSetContentSettingsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.setContentSettings",
                             UTILITIES_SET_CONTENTSETTING)
  UtilitiesSetContentSettingsFunction() = default;

 private:
  ~UtilitiesSetContentSettingsFunction() override = default;
  ResponseAction Run() override;
};

class UtilitiesIsDialogOpenFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.isDialogOpen", UTILITIES_IS_DIALOG_OPEN)
  UtilitiesIsDialogOpenFunction() = default;

 private:
  ~UtilitiesIsDialogOpenFunction() override = default;
  ResponseAction Run() override;
};

class UtilitiesFocusDialogFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.focusDialog", UTILITIES_FOCUS_DIALOG)
  UtilitiesFocusDialogFunction() = default;

 private:
  ~UtilitiesFocusDialogFunction() override = default;
  ResponseAction Run() override;
};

class UtilitiesStartChromecastFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.startChromecast",
                             UTILITIES_IS_FIRST_RUN)
  UtilitiesStartChromecastFunction() = default;

 private:
  ~UtilitiesStartChromecastFunction() override = default;
  ResponseAction Run() override;
};

class UtilitiesIsFirstRunFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.isFirstRun", UTILITIES_IS_FIRST_RUN)
  UtilitiesIsFirstRunFunction() = default;

 private:
  ~UtilitiesIsFirstRunFunction() override = default;
  ResponseAction Run() override;
};

class UtilitiesGenerateQRCodeFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.generateQRCode",
                             UTILITIES_GENERATE_QR_CODE)
  UtilitiesGenerateQRCodeFunction();

 private:
  ~UtilitiesGenerateQRCodeFunction() override;
  ResponseAction Run() override;

  void OnCodeGeneratorResponse(
      const qrcode_generator::mojom::GenerateQRCodeResponsePtr response);
  void RespondOnUiThread(std::string data_url);
  void RespondOnUiThreadForFile(base::FilePath path);

  // Remote to service instance to generate QR code images.
  std::unique_ptr<qrcode_generator::QRImageGenerator> qr_generator_;

  vivaldi::utilities::CaptureQRDestination dest_ =
      vivaldi::utilities::CAPTURE_QR_DESTINATION_DATAURL;
};

class UtilitiesGetGAPIKeyFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getGAPIKey",
                             UTILITIES_GET_GOOGLE_API_KEY)
  UtilitiesGetGAPIKeyFunction() = default;

 private:
  ~UtilitiesGetGAPIKeyFunction() override = default;
  ResponseAction Run() override;
};

class UtilitiesGetGOAuthClientIdFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getGOAuthClientId",
                             UTILITIES_GET_GOOGLE_API_CLIENT_ID)
  UtilitiesGetGOAuthClientIdFunction() = default;

 private:
  ~UtilitiesGetGOAuthClientIdFunction() override = default;
  ResponseAction Run() override;
};

class UtilitiesGetGOAuthClientSecretFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getGOAuthClientSecret",
                             UTILITIES_GET_GOOGLE_API_CLIENT_SECRET)
  UtilitiesGetGOAuthClientSecretFunction() = default;

 private:
  ~UtilitiesGetGOAuthClientSecretFunction() override = default;
  ResponseAction Run() override;
};

class UtilitiesGetMOAuthClientIdFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getMOAuthClientId",
                             UTILITIES_GET_MICROSOFT_API_CLIENT_ID)
  UtilitiesGetMOAuthClientIdFunction() = default;

 private:
  ~UtilitiesGetMOAuthClientIdFunction() override = default;
  ResponseAction Run() override;
};

class UtilitiesGetYOAuthClientIdFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getYOAuthClientId",
                             UTILITIES_GET_YAHOO_API_CLIENT_ID)
  UtilitiesGetYOAuthClientIdFunction() = default;

 private:
  ~UtilitiesGetYOAuthClientIdFunction() override = default;
  ResponseAction Run() override;
};

class UtilitiesGetYOAuthClientSecretFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getYOAuthClientSecret",
                             UTILITIES_GET_YAHOO_API_CLIENT_SECRET)
  UtilitiesGetYOAuthClientSecretFunction() = default;

 private:
  ~UtilitiesGetYOAuthClientSecretFunction() override = default;
  ResponseAction Run() override;
};

class UtilitiesGetVivaldiNetOAuthClientSecretFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getVivaldiNetOAuthClientSecret",
                             UTILITIES_GET_VIVALDINET_API_CLIENT_SECRET)
  UtilitiesGetVivaldiNetOAuthClientSecretFunction() = default;

 private:
  ~UtilitiesGetVivaldiNetOAuthClientSecretFunction() override = default;
  ResponseAction Run() override;
};

class UtilitiesGetVivaldiNetOAuthClientIdFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getVivaldiNetOAuthClientId",
                             UTILITIES_GET_VIVALDINET_API_CLIENT_ID)
  UtilitiesGetVivaldiNetOAuthClientIdFunction() = default;

 private:
  ~UtilitiesGetVivaldiNetOAuthClientIdFunction() override = default;
  ResponseAction Run() override;
};

class UtilitiesGetFOAuthClientIdFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getFOAuthClientId",
                             UTILITIES_GET_FASTMAIL_API_CLIENT_ID)
  UtilitiesGetFOAuthClientIdFunction() = default;

 private:
  ~UtilitiesGetFOAuthClientIdFunction() override = default;
  ResponseAction Run() override;
};

class UtilitiesGetCommandLineValueFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getCommandLineValue",
                             UTILITIES_GET_COMMAND_LINE_VALUE)
  UtilitiesGetCommandLineValueFunction();

 private:
  ~UtilitiesGetCommandLineValueFunction() override;
  ResponseAction Run() override;
};

class UtilitiesOsCryptFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.osCrypt", UTILITIES_OS_CRYPT)
  UtilitiesOsCryptFunction();

 private:
  ~UtilitiesOsCryptFunction() override;
  ResponseAction Run() override;

  void OnEncryptDone(std::unique_ptr<std::string> encrypted, bool result);
};

class UtilitiesOsDecryptFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.osDecrypt", UTILITIES_OS_DECRYPT)
  UtilitiesOsDecryptFunction();

 private:
  ~UtilitiesOsDecryptFunction() override;
  ResponseAction Run() override;

  void OnDecryptDone(std::unique_ptr<std::string> decrypted, bool result);
};

class UtilitiesTranslateTextFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.translateText",
                             UTILITIES_TRANSLATE_TEXT)
  UtilitiesTranslateTextFunction();

 private:
  ~UtilitiesTranslateTextFunction() override;
  ResponseAction Run() override;

  void OnTranslateFinished(::vivaldi::TranslateError error,
                           std::string detected_source_language,
                           std::vector<std::string> sourceText,
                           std::vector<std::string> translatedText);

  std::unique_ptr<::vivaldi::VivaldiTranslateServerRequest> request_;
};

class UtilitiesShowManageSSLCertificatesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.showManageSSLCertificates",
                             UTILITIES_SHOW_MANAGESSLCERTIFICATES)
  UtilitiesShowManageSSLCertificatesFunction() = default;

 private:
  ~UtilitiesShowManageSSLCertificatesFunction() override = default;
  ResponseAction Run() override;
};

class UtilitiesSetProtocolHandlingFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.setProtocolHandling",
                             UTILITIES_SET_PROTOCOL_HANDLING)
  UtilitiesSetProtocolHandlingFunction() = default;

 private:
  ~UtilitiesSetProtocolHandlingFunction() override = default;
  ResponseAction Run() override;
};

class UtilitiesConnectProxyFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.connectProxy", UTILITIES_PROXY_CONNECT)
  UtilitiesConnectProxyFunction() = default;

 private:
  ~UtilitiesConnectProxyFunction() override = default;
  ResponseAction Run() override;
};

class UtilitiesDisconnectProxyFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.disconnectProxy",
                             UTILITIES_PROXY_DISCONNECT)
  UtilitiesDisconnectProxyFunction() = default;

 private:
  ~UtilitiesDisconnectProxyFunction() override = default;
  ResponseAction Run() override;
};

class UtilitiesSupportsProxyFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.supportsProxy", UTILITIES_PROXY_SUPPORT)
  UtilitiesSupportsProxyFunction() = default;

 private:
  ~UtilitiesSupportsProxyFunction() override = default;
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_VIVALDI_UTILITIES_VIVALDI_UTILITIES_API_H_
