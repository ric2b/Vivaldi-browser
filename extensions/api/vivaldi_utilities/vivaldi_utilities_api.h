//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#ifndef EXTENSIONS_API_VIVALDI_UTILITIES_VIVALDI_UTILITIES_API_H_
#define EXTENSIONS_API_VIVALDI_UTILITIES_VIVALDI_UTILITIES_API_H_

#include <map>
#include <memory>
#include <string>

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/schema/vivaldi_utilities.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class Browser;

namespace extensions {

class VivaldiUtilitiesEventRouter {
 public:
  explicit VivaldiUtilitiesEventRouter(Profile* profile);
  ~VivaldiUtilitiesEventRouter();

  // Helper to actually dispatch an event to extension listeners.
  void DispatchEvent(const std::string& event_name,
                     std::unique_ptr<base::ListValue> event_args);

 private:
  content::BrowserContext* browser_context_;
  DISALLOW_COPY_AND_ASSIGN(VivaldiUtilitiesEventRouter);
};

class VivaldiUtilitiesAPI : public BrowserContextKeyedAPI,
                            public extensions::AppWindowRegistry::Observer,
                            public EventRouter::Observer {
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

  // Returns the window id based on the given app window id.
  Browser* FindBrowserFromAppWindowId(const std::string& appwindow_id);

  // Fires an event to JS with the active device that triggers scrolling.
  // 1: Mouse, 2: Trackpad 3: Inertial"
  void ScrollType(int scrollType);

  // Maps the given app window id to a window id.
  void MapAppWindowIdToWindowId(const std::string& appwindow_id, int window_id);

  // AppWindowRegistry::Observer
  void OnAppWindowActivated(extensions::AppWindow* app_window) override;
  void OnAppWindowRemoved(extensions::AppWindow* app_window) override;

 private:
  friend class BrowserContextKeyedAPIFactory<VivaldiUtilitiesAPI>;

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "UtilitiesAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Maps the window id of the Browser window to a AppWindow id
  typedef std::map<std::string, int> WindowIdToAppWindowId;
  WindowIdToAppWindowId appwindow_id_to_window_id_;

  std::unique_ptr<VivaldiUtilitiesEventRouter> event_router_;
};

class UtilitiesBasicPrintFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.basicPrint", UTILITIES_BASICPRINT)
  UtilitiesBasicPrintFunction();

  bool RunAsync() override;

 protected:
  ~UtilitiesBasicPrintFunction() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesBasicPrintFunction);
};

class UtilitiesClearAllRecentlyClosedSessionsFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.clearAllRecentlyClosedSessions",
                             UTILITIES_CLEARALLRECENTLYCLOSEDSESSIONS)
  UtilitiesClearAllRecentlyClosedSessionsFunction() = default;

 protected:
  ~UtilitiesClearAllRecentlyClosedSessionsFunction() override;

  bool RunAsync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesClearAllRecentlyClosedSessionsFunction);
};

// Obtains a 64-bit inbteger, encoded as 16 hex characters to be used by piwik
// as a unique user id. The user id is stored in the vivaldi user profile
// with a backup copy stored in the OS user profile (registry on Windows).
// If no stored user id is found, a new one is generated.
class UtilitiesGetUniqueUserIdFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getUniqueUserId",
                             UTILITIES_GETUNIQUEUSERID)
  UtilitiesGetUniqueUserIdFunction() = default;

 protected:
  ~UtilitiesGetUniqueUserIdFunction() override;

  bool RunAsync() override;

  bool ReadUserIdFromOSProfile(std::string* user_id);
  void WriteUserIdToOSProfile(const std::string& user_id);

  void GetUniqueUserIdOnFileThread(const std::string& legacy_user_id);
  void RespondOnUIThread(const std::string& user_id, bool is_new_user);

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesGetUniqueUserIdFunction);
};

class UtilitiesIsTabInLastSessionFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.isTabInLastSession",
                             UTILITIES_ISTABINLASTSESSION)
  UtilitiesIsTabInLastSessionFunction();

 protected:
  ~UtilitiesIsTabInLastSessionFunction() override;

  // ExtensionFunction:
  bool RunAsync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesIsTabInLastSessionFunction);
};

class UtilitiesIsUrlValidFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.isUrlValid", UTILITIES_ISURLVALID)
  UtilitiesIsUrlValidFunction();

 protected:
  ~UtilitiesIsUrlValidFunction() override;

  // ExtensionFunction:
  bool RunAsync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesIsUrlValidFunction);
};

class UtilitiesGetSelectedTextFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getSelectedText",
                             UTILITIES_GETSELECTEDTEXT)
  UtilitiesGetSelectedTextFunction();

 protected:
  ~UtilitiesGetSelectedTextFunction() override;

  // ExtensionFunction:
  bool RunAsync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesGetSelectedTextFunction);
};

class UtilitiesMapFocusAppWindowToWindowIdFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.mapFocusAppWindowToWindowId",
                             UTILITIES_MAPFOCUSAPPWINDOWTOWINDOWIDFUNCTION)
  UtilitiesMapFocusAppWindowToWindowIdFunction();

 protected:
  ~UtilitiesMapFocusAppWindowToWindowIdFunction() override;

  // ExtensionFunction:
  bool RunAsync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesMapFocusAppWindowToWindowIdFunction);
};

class UtilitiesPauseAllDownloadsFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.pauseAllDownloads",
                             UTILITIES_PAUSEALLDOWNLOADS)
  UtilitiesPauseAllDownloadsFunction() = default;

 protected:
  ~UtilitiesPauseAllDownloadsFunction() override = default;

  // ExtensionFunction:
  bool RunAsync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesPauseAllDownloadsFunction);
};

class UtilitiesCreateUrlMappingFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.createUrlMapping",
                             UTILITIES_CREATEURLMAPPING)
  UtilitiesCreateUrlMappingFunction() = default;

 protected:
  ~UtilitiesCreateUrlMappingFunction() override = default;

  // ExtensionFunction:
  bool RunAsync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesCreateUrlMappingFunction);
};

class UtilitiesRemoveUrlMappingFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.removeUrlMapping",
                             UTILITIES_REMOVEURLMAPPING)
  UtilitiesRemoveUrlMappingFunction() = default;

 protected:
  ~UtilitiesRemoveUrlMappingFunction() override = default;

  // ExtensionFunction:
  bool RunAsync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesRemoveUrlMappingFunction);
};

class UtilitiesSelectFileFunction : public ChromeAsyncExtensionFunction,
                                    public ui::SelectFileDialog::Listener {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.selectFile", UTILITIES_SELECTFILE)
  UtilitiesSelectFileFunction();

 protected:
  ~UtilitiesSelectFileFunction() override;

  // ExtensionFunction:
  bool RunAsync() override;

  // ui::SelectFileDialog::Listener:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectionCanceled(void* params) override;

 private:
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  DISALLOW_COPY_AND_ASSIGN(UtilitiesSelectFileFunction);
};

class UtilitiesGetVersionFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getVersion",
                             UTILITIES_GETVERSION)
  UtilitiesGetVersionFunction() = default;

 protected:
  ~UtilitiesGetVersionFunction() override = default;

  bool RunAsync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesGetVersionFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_VIVALDI_UTILITIES_VIVALDI_UTILITIES_API_H_
