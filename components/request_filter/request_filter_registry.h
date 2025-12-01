// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_REQUEST_FILTER_REGISTRY_H_
#define COMPONENTS_REQUEST_FILTER_REQUEST_FILTER_REGISTRY_H_

#include <memory>

#include "components/request_filter/request_filter.h"

namespace vivaldi {
class RequestFilterRegistry {
 public:
  virtual ~RequestFilterRegistry();
  virtual void AddFilter(std::unique_ptr<RequestFilter> new_filter) = 0;
  virtual void RemoveFilter(RequestFilter* filter) = 0;
  virtual void ClearCacheOnNavigation() = 0;
};
}  // namespace vivaldi

#endif  // COMPONENTS_REQUEST_FILTER_REQUEST_FILTER_REGISTRY_H_
