// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef BROWSER_VIVALDI_TRANSLATE_LANGUAGE_LIST_H_
#define BROWSER_VIVALDI_TRANSLATE_LANGUAGE_LIST_H_

#include <memory>

#include "base/timer/timer.h"
#include "base/values.h"
#include "components/web_resource/resource_request_allowed_notifier.h"

namespace content {
class BrowserContext;
}

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace base {
class ListValue;
}  // namespace base

namespace translate {

class VivaldiTranslateLanguageList
    : public web_resource::ResourceRequestAllowedNotifier::Observer {
 public:
  VivaldiTranslateLanguageList(content::BrowserContext* context);
  ~VivaldiTranslateLanguageList() override;

 private:
  void StartDownload();
  bool ShouldUpdate();
  void StartUpdateTimer();
  void OnListDownloaded(std::unique_ptr<std::string> response_body);
  void SetPrefsListAsDefault();
  void SetListInChromium(const base::ListValue& list);
  const std::string GetServer();

  content::BrowserContext* context_;

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  base::OneShotTimer update_timer_;

  // set to true when we get notified that network requests can be done.
  bool can_update_ = false;

  // ResourceRequestAllowedNotifier::Observer methods.
  void OnResourceRequestsAllowed() override;

  // Helper class to know if it's allowed to make network resource requests.
  web_resource::ResourceRequestAllowedNotifier
      resource_request_allowed_notifier_;

  base::WeakPtrFactory<VivaldiTranslateLanguageList> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiTranslateLanguageList);
};

}  // namespace vivaldi_translate

#endif  // BROWSER_TRANSLATE_VIVALDI_LANGUAGE_LIST_H_
