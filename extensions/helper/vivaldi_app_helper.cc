// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.

#include "extensions/helper/vivaldi_app_helper.h"

namespace extensions {

WEB_CONTENTS_USER_DATA_KEY_IMPL(VivaldiAppHelper);

VivaldiAppHelper::VivaldiAppHelper(content::WebContents* contents)
    : content::WebContentsUserData<VivaldiAppHelper>(*contents) {}

VivaldiAppHelper::~VivaldiAppHelper() {}

}  // namespace extensions
