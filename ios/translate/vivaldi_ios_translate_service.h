// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TRANSLATE_VIVALDI_IOS_TRANSLATE_SERVICE_H_
#define IOS_TRANSLATE_VIVALDI_IOS_TRANSLATE_SERVICE_H_

#include <string>

#import "base/timer/timer.h"
#import "base/values.h"
#import "components/web_resource/resource_request_allowed_notifier.h"

class GURL;
class PrefService;

namespace language {
class LanguageModel;
}  // namespace language

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

// Singleton managing the resources required for Translate.
class VivaldiIOSTranslateService
    : public web_resource::ResourceRequestAllowedNotifier::Observer {
 public:
  // Must be called before the Translate feature can be used.
  static void Initialize();

  // Must be called to shut down the Translate feature.
  static void Shutdown();

  // Returns the language to translate to. For more details, see
  // TranslateManager::GetTargetLanguage.
  static std::string GetTargetLanguage(PrefService* prefs,
                                       language::LanguageModel* language_model);

  // Returns true if the URL can be translated.
  static bool IsTranslatableURL(const GURL& url);

  VivaldiIOSTranslateService(const VivaldiIOSTranslateService&) = delete;
  VivaldiIOSTranslateService& operator=(const VivaldiIOSTranslateService&) = delete;

 private:
  VivaldiIOSTranslateService();
  ~VivaldiIOSTranslateService() override;

  void StartDownload();
  bool ShouldUpdate();
  void StartUpdateTimer();
  void OnListDownloaded(std::unique_ptr<std::string> response_body);
  void SetPrefsListAsDefault();
  void SetListInChromium(const base::Value::List& list);
  const std::string GetServer();

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  base::OneShotTimer update_timer_;

  // set to true when we get notified that network requests can be done.
  bool can_update_ = false;

  // ResourceRequestAllowedNotifier::Observer methods.
  void OnResourceRequestsAllowed() override;

  // Helper class to know if it's allowed to make network resource requests.
  web_resource::ResourceRequestAllowedNotifier
      resource_request_allowed_notifier_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::WeakPtrFactory<VivaldiIOSTranslateService> weak_factory_;
};

#endif  // IOS_TRANSLATE_VIVALDI_IOS_TRANSLATE_SERVICE_H_
