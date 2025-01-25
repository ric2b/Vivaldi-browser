// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.

#include "prefs/vivaldi_tab_zoom_pref.h"

#include "app/vivaldi_apptools.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents_user_data.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

// NOTE(arnar@vivaldi.com): Helper function to determine if Vivaldi tab zoom
// is enabled.
namespace vivaldi {

bool IsTabZoomEnabled(content::WebContents* web_contents) {
  if (!::vivaldi::IsVivaldiRunning())
    return false;

  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  if (!browser_context)
    return false;

  Profile* profile = static_cast<Profile*>(browser_context);

  bool tabZoom =
      profile->GetPrefs()->GetBoolean(vivaldiprefs::kWebpagesTabZoomEnabled);
  return tabZoom;
}

}  // namespace vivaldi
