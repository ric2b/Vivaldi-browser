// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "browser/ui/vivaldi_tab_helpers.h"

#include "app/vivaldi_apptools.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_filter/chrome_content_subresource_filter_web_contents_helper_factory.h"

#include "components/adverse_adblocking/adverse_ad_filter_list.h"
#include "components/adverse_adblocking/adverse_ad_filter_list_factory.h"
#include "components/adverse_adblocking/vivaldi_subresource_filter_throttle_manager.h"
#include "components/bookmarks/bookmark_thumbnail_theme_tab_helper.h"
#include "components/prefs/pref_service.h"

#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"

#include "extensions/buildflags/buildflags.h"
#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/api/tabs/tabs_private_api.h"
#endif
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/device/public/mojom/geolocation_context.mojom.h"
#include "services/device/public/mojom/geoposition.mojom.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

using content::WebContents;

namespace vivaldi {
void VivaldiAttachTabHelpers(WebContents* web_contents) {
  if (vivaldi::IsVivaldiRunning() || vivaldi::ForcedVivaldiRunning()) {
    VivaldiSubresourceFilterAdblockingThrottleManager::
        CreateSubresourceFilterWebContentsHelper(web_contents);

    AdverseAdFilterListService* adblock_list =
        VivaldiAdverseAdFilterListFactory::GetForProfile(
            Profile::FromBrowserContext(web_contents->GetBrowserContext()));
    VivaldiSubresourceFilterAdblockingThrottleManager::FromWebContents(
        web_contents)
        ->set_adblock_list(adblock_list);

    CreateSubresourceFilterWebContentsHelper(web_contents);

    vivaldi_bookmark_kit::BookmarkThumbnailThemeTabHelper::CreateForWebContents(
        web_contents);
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (vivaldi::IsVivaldiRunning() || vivaldi::ForcedVivaldiRunning()) {
    extensions::VivaldiPrivateTabObserver::CreateForWebContents(web_contents);
    // Attach a contentsobserver to update the renderer prefs we want to change.
    extensions::VivaldiGuestViewContentObserver::CreateForWebContents(
        web_contents);
  }
#endif

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile->GetPrefs()->GetBoolean(vivaldiprefs::kGeolocationUseOverride)) {
    double latitude =
        profile->GetPrefs()->GetDouble(vivaldiprefs::kGeolocationLatitude);
    double longitude =
        profile->GetPrefs()->GetDouble(vivaldiprefs::kGeolocationLongitude);
    double accuracy =
        profile->GetPrefs()->GetDouble(vivaldiprefs::kGeolocationAccuracy);

    content::WebContentsImpl* web_contents_impl =
        static_cast<content::WebContentsImpl*>(web_contents);

    auto* geolocation_context = web_contents_impl->GetGeolocationContext();

    device::mojom::GeopositionResultPtr geoposition;
    auto position = device::mojom::Geoposition::New();
    position->latitude = latitude;
    position->longitude = longitude;
    position->accuracy = accuracy;
    position->timestamp = base::Time::Now();
    if (device::ValidateGeoposition(*position)) {
      geoposition =
          device::mojom::GeopositionResult::NewPosition(std::move(position));
    } else {
      geoposition = device::mojom::GeopositionResult::NewError(
          device::mojom::GeopositionError::New(
              device::mojom::GeopositionErrorCode::kPositionUnavailable,
              /*error_message=*/"", /*error_technical=*/""));
    }
    geolocation_context->SetOverride(std::move(geoposition));
  }
}
}  // namespace vivaldi
