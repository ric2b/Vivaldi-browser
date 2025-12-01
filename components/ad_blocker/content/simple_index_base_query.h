// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_CONTENT_SIMPLE_INDEX_BASE_QUERY_H_
#define COMPONENTS_AD_BLOCKER_CONTENT_SIMPLE_INDEX_BASE_QUERY_H_

#include "components/ad_blocker/content/index/adblock_rules_index.h"

namespace adblock_filter {

class SimpleIndexBaseQuery : public virtual RulesIndex::BaseQuery {
 public:
  SimpleIndexBaseQuery(GURL url, url::Origin origin);
  ~SimpleIndexBaseQuery() override;

  SimpleIndexBaseQuery(SimpleIndexBaseQuery&&);
  SimpleIndexBaseQuery& operator=(SimpleIndexBaseQuery&&);

  const GURL& GetUrl() const override;
  const url::Origin& GetOrigin() const override;

  bool IsThirdParty() const override;
  bool IsStrictThirdParty() const override;

 private:
  GURL url_;
  url::Origin origin_;
  bool is_third_party_;
  bool is_strict_third_party_;
};
}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_CONTENT_SIMPLE_INDEX_BASE_QUERY_H_
