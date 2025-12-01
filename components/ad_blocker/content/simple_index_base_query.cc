// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/content/simple_index_base_query.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace adblock_filter {
SimpleIndexBaseQuery::SimpleIndexBaseQuery(GURL url, url::Origin origin)
    : url_(std::move(url)),
      origin_(std::move(origin)),
      is_third_party_(
          origin.opaque() ||
          !net::registry_controlled_domains::SameDomainOrHost(
              url,
              origin,
              net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)),
      is_strict_third_party_(origin.IsSameOriginWith(url)) {}

SimpleIndexBaseQuery::~SimpleIndexBaseQuery() = default;
SimpleIndexBaseQuery::SimpleIndexBaseQuery(SimpleIndexBaseQuery&&) = default;
SimpleIndexBaseQuery& SimpleIndexBaseQuery::operator=(SimpleIndexBaseQuery&&) =
    default;

const GURL& SimpleIndexBaseQuery::GetUrl() const {
  return url_;
}

const url::Origin& SimpleIndexBaseQuery::GetOrigin() const {
  return origin_;
}

bool SimpleIndexBaseQuery::IsThirdParty() const {
  return is_third_party_;
}

bool SimpleIndexBaseQuery::IsStrictThirdParty() const {
  return is_strict_third_party_;
}

}  // namespace adblock_filter
