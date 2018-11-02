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

  // Fires an event to JS with the active device that triggers scrolling.
  // 1: Mouse, 2: Trackpad 3: Inertial"
  void ScrollType(int scrollType);

  // Returns true if the key didn't not exist previously, false if it updated
  // an existing value
  bool SetSharedData(const std::string& key, base::Value* value);

  // Looks up an existing key/value pair, returns nullptr if the key does not
  // exist.
  const base::Value* GetSharedData(const std::string& key);

 private:
  friend class BrowserContextKeyedAPIFactory<VivaldiUtilitiesAPI>;

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "UtilitiesAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  std::unique_ptr<VivaldiUtilitiesEventRouter> event_router_;

  // Map used for the *sharedData apis.
  std::map<std::string, base::Value*> key_to_values_map_;
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

class UtilitiesSetSharedDataFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.setSharedData", UTILITIES_SETSHAREDDATA)

  UtilitiesSetSharedDataFunction() = default;

 protected:
  ~UtilitiesSetSharedDataFunction() override = default;

  bool RunAsync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesSetSharedDataFunction);
};

class UtilitiesGetSharedDataFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getSharedData", UTILITIES_GETSHAREDDATA)

  UtilitiesGetSharedDataFunction() = default;

 protected:
  ~UtilitiesGetSharedDataFunction() override = default;

  bool RunAsync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesGetSharedDataFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_VIVALDI_UTILITIES_VIVALDI_UTILITIES_API_H_
