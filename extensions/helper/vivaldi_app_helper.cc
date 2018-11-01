// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.

#include "extensions/helper/vivaldi_app_helper.h"

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "extensions/api/tabs/tabs_private_api.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(extensions::VivaldiAppHelper);

namespace extensions {

VivaldiAppHelper::VivaldiAppHelper(content::WebContents* contents) {}

VivaldiAppHelper::~VivaldiAppHelper() {}

}  // namespace extensions
