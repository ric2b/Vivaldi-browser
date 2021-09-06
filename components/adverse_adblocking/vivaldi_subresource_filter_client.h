// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_ADVERSE_ADBLOCKING_VIVALDI_SUBRESOURCE_FILTER_CLIENT_H_
#define COMPONENTS_ADVERSE_ADBLOCKING_VIVALDI_SUBRESOURCE_FILTER_CLIENT_H_

#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"

#include "components/subresource_filter/content/browser/subresource_filter_client.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationThrottle;
}

namespace base {
class SingleThreadTaskRunner;
}

class AdverseAdFilterListService;

namespace subresource_filter {
class ContentSubresourceFilterThrottleManager;
class SubresourceFilterProfileContext;
}  // namespace subresource_filter

class SubresourceFilterContentSettingsManager;

class VivaldiSubresourceFilterClient
    : public subresource_filter::SubresourceFilterClient {
 public:
  VivaldiSubresourceFilterClient(
      content::WebContents* web_contents,
      ChromeSubresourceFilterClient* csfc);
  ~VivaldiSubresourceFilterClient() override;

  // Creates a ContentSubresourceFilterThrottleManager and attaches it to
  // |web_contents|, passing it an instance of this client and other
  // embedder-level state.
  static void CreateThrottleManagerWithClientForWebContents(
      content::WebContents* web_contents);

  // Returns the VivaldiSubresourceFilterClient instance that is owned by the
  // ThrottleManager owned by |web_contents|, or nullptr if there is no such
  // ThrottleManager.
  static VivaldiSubresourceFilterClient* FromWebContents(
      content::WebContents* web_contents);

  void MaybeAppendNavigationThrottles(
      content::NavigationHandle* navigation_handle,
      std::vector<std::unique_ptr<content::NavigationThrottle>>* throttles);

  // SubresourceFilterClient:
  void ShowNotification() override;

  const scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
  GetSafeBrowsingDatabaseManager() override;

  void SetAdblockList(AdverseAdFilterListService* list) {
    adblock_list_ = list;
  }
  AdverseAdFilterListService* adblock_list() { return adblock_list_; }

 private:

  // io task runner
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Used to chain WebContents attachments, make this a unique pointer.
  std::unique_ptr<ChromeSubresourceFilterClient> chrome_subresource_filter_client_;

  AdverseAdFilterListService* adblock_list_ = nullptr;  // owned by the profile.

  subresource_filter::SubresourceFilterProfileContext* profile_context_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(VivaldiSubresourceFilterClient);
};

#endif  // COMPONENTS_ADVERSE_ADBLOCKING_VIVALDI_SUBRESOURCE_FILTER_CLIENT_H_
