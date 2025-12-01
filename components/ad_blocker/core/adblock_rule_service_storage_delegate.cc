// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/core/adblock_rule_service_storage_delegate.h"

namespace adblock_filter {

RuleServiceStorageDelegate::~RuleServiceStorageDelegate() = default;

RuleServiceStorageDelegate::LoadResult::LoadResult() = default;
RuleServiceStorageDelegate::LoadResult::~LoadResult() = default;
RuleServiceStorageDelegate::LoadResult::LoadResult(LoadResult&& load_result) =
    default;
RuleServiceStorageDelegate::LoadResult&
RuleServiceStorageDelegate::LoadResult::operator=(LoadResult&& load_result) =
    default;

}  // namespace adblock_filter
