// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_PERMISSION_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_PERMISSION_MANAGER_H_

#include <map>
#include <memory>

#include "android_webview/browser/aw_context_permissions_delegate.h"
#include "base/containers/id_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_result.h"

namespace blink {
enum class PermissionType;
}

namespace android_webview {

class AwBrowserPermissionRequestDelegate;
class LastRequestResultCache;

class AwPermissionManager : public content::PermissionControllerDelegate {
 public:
  explicit AwPermissionManager(
      const AwContextPermissionsDelegate& context_delegate);

  AwPermissionManager(const AwPermissionManager&) = delete;
  AwPermissionManager& operator=(const AwPermissionManager&) = delete;

  ~AwPermissionManager() override;

  // PermissionControllerDelegate implementation.
  void RequestPermissions(
      content::RenderFrameHost* render_frame_host,
      const content::PermissionRequestDescription& request_description,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback)
      override;
  void ResetPermission(blink::PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override;
  void RequestPermissionsFromCurrentDocument(
      content::RenderFrameHost* render_frame_host,
      const content::PermissionRequestDescription& request_description,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback)
      override;
  blink::mojom::PermissionStatus GetPermissionStatus(
      blink::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  content::PermissionResult GetPermissionResultForOriginWithoutContext(
      blink::PermissionType permission,
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForCurrentDocument(
      blink::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      bool should_include_device_status) override;
  blink::mojom::PermissionStatus GetPermissionStatusForWorker(
      blink::PermissionType permission,
      content::RenderProcessHost* render_process_host,
      const GURL& worker_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForEmbeddedRequester(
      blink::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const url::Origin& requesting_origin) override;
  SubscriptionId SubscribeToPermissionStatusChange(
      blink::PermissionType permission,
      content::RenderProcessHost* render_process_host,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool should_include_device_status,
      base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
  void UnsubscribeFromPermissionStatusChange(
      SubscriptionId subscription_id) override;
  void SetOriginCanReadEnumerateDevicesAudioLabels(const url::Origin& origin,
                                                   bool audio);
  void SetOriginCanReadEnumerateDevicesVideoLabels(const url::Origin& origin,
                                                   bool video);
  bool ShouldShowEnumerateDevicesAudioLabels(const url::Origin& origin);
  bool ShouldShowEnumerateDevicesVideoLabels(const url::Origin& origin);
  void ClearEnumerateDevicesCachedPermission(const url::Origin& origin,
                                             bool remove_audio,
                                             bool remove_video);

 protected:
  void CancelPermissionRequest(int request_id);
  void CancelPermissionRequests();

 private:
  class PendingRequest;
  using PendingRequestsMap = base::IDMap<std::unique_ptr<PendingRequest>>;

  virtual int GetRenderProcessID(content::RenderFrameHost* render_frame_host);
  virtual int GetRenderFrameID(content::RenderFrameHost* render_frame_host);
  virtual GURL LastCommittedMainOrigin(
      content::RenderFrameHost* render_frame_host);
  virtual AwBrowserPermissionRequestDelegate* GetDelegate(int render_process_id,
                                                          int render_frame_id);

  blink::mojom::PermissionStatus GetPermissionStatusInternal(
      blink::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      content::WebContents* web_contents);

  blink::mojom::PermissionStatus GetGeolocationPermission(
      const GURL& requesting_origin,
      content::WebContents* web_contents);

  // The weak pointer to this is used to clean up any information which is
  // stored in the pending request or result cache maps. However, the callback
  // should be run regardless of whether the class is still alive so the method
  // is static.
  static void OnRequestResponse(
      const base::WeakPtr<AwPermissionManager>& manager,
      int request_id,
      blink::PermissionType permission,
      bool allowed);

  base::raw_ref<const AwContextPermissionsDelegate> context_delegate_;
  PendingRequestsMap pending_requests_;
  std::unique_ptr<LastRequestResultCache> result_cache_;
  // Maps origins to whether they can view device labels.
  // The pair is ordered as (Audio, Video).
  std::map<url::Origin, std::pair<bool, bool>> enumerate_devices_labels_cache_;

  base::WeakPtrFactory<AwPermissionManager> weak_ptr_factory_{this};
};

} // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_PERMISSION_MANAGER_H_
