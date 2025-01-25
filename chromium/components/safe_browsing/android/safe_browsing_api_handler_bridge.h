// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Glue to pass Safe Browsing API requests between Chrome and GMSCore.

#ifndef COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_BRIDGE_H_
#define COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_BRIDGE_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"

class GURL;

namespace safe_browsing {

class UrlCheckInterceptor;
struct ThreatMetadata;

class SafeBrowsingApiHandlerBridge {
 public:
  using ResponseCallback =
      base::OnceCallback<void(SBThreatType, const ThreatMetadata&)>;
  using VerifyAppsResponseCallback =
      base::OnceCallback<void(VerifyAppsEnabledResult)>;

  SafeBrowsingApiHandlerBridge() = default;

  ~SafeBrowsingApiHandlerBridge();

  SafeBrowsingApiHandlerBridge(const SafeBrowsingApiHandlerBridge&) = delete;
  SafeBrowsingApiHandlerBridge& operator=(const SafeBrowsingApiHandlerBridge&) =
      delete;

  // Returns a reference to the singleton.
  static SafeBrowsingApiHandlerBridge& GetInstance();

  // Makes Native-to-Java call to perform the hash-prefix database check.
  void StartHashDatabaseUrlCheck(std::unique_ptr<ResponseCallback> callback,
                                 const GURL& url,
                                 const SBThreatTypeSet& threat_types);

  // Makes Native-to-Java call to perform the privacy-preserving hash real-time
  // check.
  void StartHashRealTimeUrlCheck(std::unique_ptr<ResponseCallback> callback,
                                 const GURL& url,
                                 const SBThreatTypeSet& threat_types);

  bool StartCSDAllowlistCheck(const GURL& url);

  // Query whether app verification is enabled. Will run `callback` with
  // the result of the query.
  void StartIsVerifyAppsEnabled(VerifyAppsResponseCallback callback);

  // Prompt the user to enable app verification. Will run `callback`
  // with the result of the query.
  void StartEnableVerifyApps(VerifyAppsResponseCallback callback);

  // Called when a non-recoverable failure is encountered from SafeBrowsing API.
  void OnSafeBrowsingApiNonRecoverableFailure();

  void SetInterceptorForTesting(UrlCheckInterceptor* interceptor) {
    interceptor_for_testing_ = interceptor;
  }

  void ResetSafeBrowsingApiAvailableForTesting() {
    is_safe_browsing_api_available_ = true;
  }

  void SetVerifyAppsEnableResultForTesting(VerifyAppsEnabledResult result) {
    verify_apps_enabled_for_testing_ = result;
  }

 private:
  // Makes Native-to-Java call to check the URL through GMSCore SafetyNet API.
  void StartUrlCheckBySafetyNet(std::unique_ptr<ResponseCallback> callback,
                                const GURL& url,
                                const SBThreatTypeSet& threat_types);

  // Makes Native-to-Java call to check the URL through GMSCore SafeBrowsing
  // API.
  void StartUrlCheckBySafeBrowsing(std::unique_ptr<ResponseCallback> callback,
                                   const GURL& url,
                                   const SBThreatTypeSet& threat_types,
                                   const SafeBrowsingJavaProtocol& protocol);

  // Used as a key to identify unique requests sent to Java to get Safe Browsing
  // reputation from GmsCore SafetyNet API.
  jlong next_safety_net_callback_id_ = 0;

  // Used as a key to identify unique requests sent to Java to get Safe Browsing
  // reputation from GmsCore SafeBrowsing API.
  jlong next_safe_browsing_callback_id_ = 0;

  // Used as a key to identify unique requests sent to Java related to
  // SafetyNet app verification.
  jlong next_verify_apps_callback_id_ = 0;

  // Whether SafeBrowsing API is available. Set to false if previous call to
  // SafeBrowsing API has encountered a non-recoverable failure. If set to
  // false, future calls to SafeBrowsing API will fall back to SafetyNet API.
  // Once set to false, it will remain false until browser restarts.
  bool is_safe_browsing_api_available_ = true;

  raw_ptr<UrlCheckInterceptor> interceptor_for_testing_ = nullptr;

  std::optional<VerifyAppsEnabledResult> verify_apps_enabled_for_testing_ =
      std::nullopt;
};

// Interface allowing simplified interception of calls to
// SafeBrowsingApiHandlerBridge. Intended for use only in tests.
class UrlCheckInterceptor {
 public:
  virtual ~UrlCheckInterceptor() = default;
  virtual void CheckBySafetyNet(
      std::unique_ptr<SafeBrowsingApiHandlerBridge::ResponseCallback> callback,
      const GURL& url) = 0;
  virtual void CheckBySafeBrowsing(
      std::unique_ptr<SafeBrowsingApiHandlerBridge::ResponseCallback> callback,
      const GURL& url) = 0;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_BRIDGE_H_
