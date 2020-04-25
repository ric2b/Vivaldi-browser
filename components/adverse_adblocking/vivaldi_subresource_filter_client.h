// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_ADVERSE_ADBLOCKING_VIVALDI_SUBRESOURCE_FILTER_CLIENT_H_
#define COMPONENTS_ADVERSE_ADBLOCKING_VIVALDI_SUBRESOURCE_FILTER_CLIENT_H_

#include "components/subresource_filter/content/browser/subresource_filter_client.h"
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
}  // namespace subresource_filter

class SubresourceFilterContentSettingsManager;

class VivaldiSubresourceFilterClient
    : public content::WebContentsObserver,
      public content::WebContentsUserData<VivaldiSubresourceFilterClient>,
      public subresource_filter::SubresourceFilterClient,
      public base::SupportsWeakPtr<VivaldiSubresourceFilterClient> {
 public:
  explicit VivaldiSubresourceFilterClient(content::WebContents* web_contents);
  ~VivaldiSubresourceFilterClient() override;

  void MaybeAppendNavigationThrottles(
      content::NavigationHandle* navigation_handle,
      std::vector<std::unique_ptr<content::NavigationThrottle>>* throttles);

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  // SubresourceFilterClient:
  void ShowNotification() override;
  subresource_filter::mojom::ActivationLevel OnPageActivationComputed(
      content::NavigationHandle* navigation_handle,
      subresource_filter::mojom::ActivationLevel initial_activation_level,
      subresource_filter::ActivationDecision* decision) override;

  bool did_show_ui_for_navigation() const;
  void SetAdblockList(AdverseAdFilterListService* list) {
    adblock_list_ = list;
  }
  AdverseAdFilterListService* adblock_list() { return adblock_list_; }

 private:
  friend class content::WebContentsUserData<VivaldiSubresourceFilterClient>;

  std::unique_ptr<subresource_filter::ContentSubresourceFilterThrottleManager>
      throttle_manager_;

  // io task runner
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  AdverseAdFilterListService* adblock_list_ = nullptr;  // owned by the profile.

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(VivaldiSubresourceFilterClient);
};

#endif  // COMPONENTS_ADVERSE_ADBLOCKING_VIVALDI_SUBRESOURCE_FILTER_CLIENT_H_
