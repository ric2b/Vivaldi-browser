// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/request_filter.h"

namespace vivaldi {
RequestFilter::RequestHeaderChanges::RequestHeaderChanges() = default;
RequestFilter::RequestHeaderChanges::~RequestHeaderChanges() = default;
RequestFilter::RequestHeaderChanges::RequestHeaderChanges(
    RequestHeaderChanges&&) = default;
RequestFilter::RequestHeaderChanges&
RequestFilter::RequestHeaderChanges::operator=(RequestHeaderChanges&&) =
    default;

RequestFilter::ResponseHeaderChanges::ResponseHeaderChanges() = default;
RequestFilter::ResponseHeaderChanges::~ResponseHeaderChanges() = default;

RequestFilter::ResponseHeaderChanges::ResponseHeaderChanges(
    ResponseHeaderChanges&&) = default;
RequestFilter::ResponseHeaderChanges&
RequestFilter::ResponseHeaderChanges::operator=(ResponseHeaderChanges&&) =
    default;

RequestFilter::RequestFilter(Type type, int priority)
    : type_(type), priority_(priority) {}

RequestFilter::~RequestFilter() = default;
}  // namespace vivaldi
