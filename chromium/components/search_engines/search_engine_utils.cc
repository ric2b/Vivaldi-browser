// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/search_engines/search_engine_utils.h"
#include "components/google/core/common/google_util.h"
#include "components/search_engines/prepopulated_engines.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

#include "components/search_engines/search_engines_manager.h"

namespace SearchEngineUtils {

namespace {

bool SameDomain(const GURL& given_url, const GURL& prepopulated_url) {
  return prepopulated_url.is_valid() &&
         net::registry_controlled_domains::SameDomainOrHost(
             given_url, prepopulated_url,
             net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace

// Global functions -----------------------------------------------------------

SearchEngineType GetEngineType(const GURL& url) {
  DCHECK(url.is_valid());
  auto &engines = SearchEnginesManager::GetInstance()->GetAllEngines();

  // Check using TLD+1s, in order to more aggressively match search engine types
  // for data imported from other browsers.
  //
  // First special-case Google, because the prepopulate URL for it will not
  // convert to a GURL and thus won't have an origin.  Instead see if the
  // incoming URL's host is "[*.]google.<TLD>".
  if (google_util::IsGoogleDomainUrl(url, google_util::DISALLOW_SUBDOMAIN,
                                     google_util::ALLOW_NON_STANDARD_PORTS))
    return SearchEnginesManager::GetInstance()->GetGoogleEngine()->type;

  // Now check the rest of the prepopulate data.
  for (size_t i = 0; i < engines.size(); ++i) {
    // First check the main search URL.
    if (SameDomain(
            url, GURL(engines[i]->search_url)))
      return engines[i]->type;

    // Then check the alternate URLs.
    for (size_t j = 0;
         j < engines[i]->alternate_urls_size;
         ++j) {
      if (SameDomain(url, GURL(engines[i]->alternate_urls[j])))
        return engines[i]->type;
    }
  }

  return SEARCH_ENGINE_OTHER;
}

}  // namespace SearchEngineUtils
