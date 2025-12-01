// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/content/utils.h"

#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace adblock_filter {

bool CanFilterUrl(const GURL& url, bool is_popup) {
  // Ignore URLs that are greater than the max URL length. Since those will be
  // disallowed elsewhere in the loading stack, we can save compute time by
  // avoiding matching here.
  if (!url.is_valid() || url.spec().length() > url::kMaxURLChars) {
    return false;
  }

  return (url.SchemeIs(url::kFtpScheme) || url.SchemeIsHTTPOrHTTPS() ||
          url.SchemeIsWSOrWSS() || (is_popup && url.IsAboutBlank()));
}

}  // namespace adblock_filter
