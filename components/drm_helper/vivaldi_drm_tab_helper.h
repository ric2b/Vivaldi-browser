#ifndef COMPONENTS_DRM_HELPER_VIVALDI_DRM_TAB_HELPER_H
#define COMPONENTS_DRM_HELPER_VIVALDI_DRM_TAB_HELPER_H

#include "components/component_updater/component_updater_service.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "renderer/mojo/vivaldi_encrypted_media_access.mojom.h"

#if !BUILDFLAG(IS_LINUX)
#include "base/scoped_observation.h"
#endif

namespace drm_helper {

class DRMContentTabHelper
    : public vivaldi::mojom::VivaldiEncryptedMediaAccess,
      public update_client::UpdateClient::Observer,
      public content::WebContentsObserver,
      public content::WebContentsUserData<DRMContentTabHelper> {
 public:
  static void Create(content::RenderFrameHost* frame_host,
                     mojo::PendingAssociatedReceiver<
                         vivaldi::mojom::VivaldiEncryptedMediaAccess> receiver);

  ~DRMContentTabHelper() override;

 private:
  explicit DRMContentTabHelper(content::WebContents* contents);
  friend class content::WebContentsUserData<DRMContentTabHelper>;

  void NotifyEncryptedMediaAccess(const std::string& key_system) override;

  // Implements the ComponentUpdateService::Observer interface.
  void OnEvent(const update_client::CrxUpdateItem& item) override;

  // Receivers associated with this instance.
  content::RenderFrameHostReceiverSet<
      vivaldi::mojom::VivaldiEncryptedMediaAccess>
      media_access_receivers_;

#if !BUILDFLAG(IS_LINUX)
  void HandleModuleUpdated();

  base::ScopedObservation<component_updater::ComponentUpdateService,
                          component_updater::ComponentUpdateService::Observer>
      observer_{this};

  bool was_requested_ =
      false;  // True for instances where request for widevine happened.
#endif

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace drm_helper

#endif /* COMPONENTS_DRM_HELPER_VIVALDI_DRM_TAB_HELPER_H */
