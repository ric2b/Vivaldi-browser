// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_CONTENT_SIMPLE_INDEX_REQUEST_QUERY_H_
#define COMPONENTS_AD_BLOCKER_CONTENT_SIMPLE_INDEX_REQUEST_QUERY_H_

#include "components/ad_blocker/content/index/adblock_rules_index.h"
#include "components/ad_blocker/content/simple_index_base_query.h"

namespace adblock_filter {
class SimpleIndexRequestQuery : public SimpleIndexBaseQuery,
                                public RulesIndex::RequestQuery {
 public:
  SimpleIndexRequestQuery(GURL url,
                          url::Origin origin,
                          std::string method,
                          bool disable_generic_rules);
  ~SimpleIndexRequestQuery() override;

  SimpleIndexRequestQuery(SimpleIndexRequestQuery&&);
  SimpleIndexRequestQuery& operator=(SimpleIndexRequestQuery&&);

  std::string_view GetMethod() const override;
  bool WantsDisableGenericRules() const override;

 private:
  std::string method_;
  bool disable_generic_rules_;
};
}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_CONTENT_SIMPLE_INDEX_REQUEST_QUERY_H_
