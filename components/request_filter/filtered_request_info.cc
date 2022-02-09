// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/filtered_request_info.h"

namespace vivaldi {

FilteredRequestInfo::FilteredRequestInfo(
    uint64_t request_id,
    int render_process_id,
    int render_frame_id,
    int32_t view_routing_id,
    const network::ResourceRequest& request,
    content::ContentBrowserClient::URLLoaderFactoryType loader_factory_type,
    bool is_async,
    bool is_webtransport,
    absl::optional<int64_t> navigation_id)
    : id(request_id),
      request(request),
      view_routing_id(view_routing_id),
      render_process_id(render_process_id),
      render_frame_id(render_frame_id),
      loader_factory_type(loader_factory_type),
      is_async(is_async),
      is_webtransport(is_webtransport),
      navigation_id(std::move(navigation_id)) {}

FilteredRequestInfo::FilteredRequestInfo(FilteredRequestInfo&&) = default;

FilteredRequestInfo::~FilteredRequestInfo() = default;

void FilteredRequestInfo::AddResponse(
    const network::mojom::URLResponseHead& response) {
  this->response = response.Clone();
}

}  // namespace vivaldi
