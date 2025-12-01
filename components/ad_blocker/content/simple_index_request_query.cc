// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/content/simple_index_request_query.h"

namespace adblock_filter {

SimpleIndexRequestQuery::SimpleIndexRequestQuery(GURL url,
                                                 url::Origin origin,
                                                 std::string method,
                                                 bool disable_generic_rules)
    : SimpleIndexBaseQuery(std::move(url), std::move(origin)),
      method_(std::move(method)),
      disable_generic_rules_(disable_generic_rules) {}

SimpleIndexRequestQuery::~SimpleIndexRequestQuery() = default;

SimpleIndexRequestQuery::SimpleIndexRequestQuery(SimpleIndexRequestQuery&&) =
    default;
SimpleIndexRequestQuery& SimpleIndexRequestQuery::operator=(
    SimpleIndexRequestQuery&&) = default;

std::string_view SimpleIndexRequestQuery::GetMethod() const {
  return method_;
}

bool SimpleIndexRequestQuery::WantsDisableGenericRules() const {
  return disable_generic_rules_;
}

}  // namespace adblock_filter
