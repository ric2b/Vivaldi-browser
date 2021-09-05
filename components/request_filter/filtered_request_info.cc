// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/filtered_request_info.h"

namespace vivaldi {

FilteredRequestInfo::FilteredRequestInfo(
    uint64_t request_id,
    int initiator_render_process_id,
    int initiator_render_frame_id,
    int32_t routing_id,
    int render_process_id,
    int render_frame_id,
    const network::ResourceRequest& request,
    content::ContentBrowserClient::URLLoaderFactoryType loader_factory_type,
    bool is_async,
    base::Optional<int64_t> navigation_id)
    : id(request_id),
      request(request),
      initiator_render_process_id(initiator_render_process_id),
      initiator_render_frame_id(initiator_render_frame_id),
      routing_id(routing_id),
      render_process_id(render_process_id),
      render_frame_id(render_frame_id),
      loader_factory_type(loader_factory_type),
      is_async(is_async),
      navigation_id(std::move(navigation_id)) {}

FilteredRequestInfo::~FilteredRequestInfo() = default;

void FilteredRequestInfo::AddResponse(
    const network::mojom::URLResponseHead& response) {
  this->response = response.Clone();
}

}  // namespace vivaldi
