// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_CONTENT_UTILS_H_
#define COMPONENTS_AD_BLOCKER_CONTENT_UTILS_H_

#include "url/origin.h"

class GURL;

namespace adblock_filter {
bool CanFilterUrl(const GURL& url, bool is_popup);
}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_CONTENT_UTILS_H_
