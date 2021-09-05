// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_FILTERED_REQUEST_INFO_H_
#define COMPONENTS_REQUEST_FILTER_FILTERED_REQUEST_INFO_H_

#include <stdint.h>

#include "content/public/browser/content_browser_client.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace vivaldi {
// A URL request representation used by the request filter. This structure
// carries information about an in-progress request.
struct FilteredRequestInfo {
  explicit FilteredRequestInfo(
      uint64_t request_id,
      int initiator_render_process_id,
      int initiator_frame_id,
      int32_t routing_id,
      int render_process_id,
      int render_frame_id,
      const network::ResourceRequest& request,
      content::ContentBrowserClient::URLLoaderFactoryType loader_factory_type,
      bool is_async,
      base::Optional<int64_t> navigation_id);

  ~FilteredRequestInfo();

  // Fill in response data for this request.
  void AddResponse(const network::mojom::URLResponseHead& response);

  // A unique identifier for this request.
  const uint64_t id;

  const network::ResourceRequest request;
  network::mojom::URLResponseHeadPtr response;

  // The ID of the render process which initiated the request, or -1 of not
  // applicable (i.e. if initiated by the browser).
  const int initiator_render_process_id;

  // The render frame ID of the frame which initiated this request, or -1 if
  // the request was not initiated by a frame.
  const int initiator_render_frame_id;

  // The routing ID of the object which initiated the request, if applicable.
  const int routing_id = MSG_ROUTING_NONE;

  // The ID of the render process which runs the frame where the request
  // happens.
  const int render_process_id;

  // The ID of the frame where the request happens.
  const int render_frame_id;

  const content::ContentBrowserClient::URLLoaderFactoryType loader_factory_type;

  // Indicates if this request is asynchronous.
  const bool is_async;

  // Valid if this request corresponds to a navigation.
  const base::Optional<int64_t> navigation_id;

 private:
  DISALLOW_COPY_AND_ASSIGN(FilteredRequestInfo);
};

}  // namespace vivaldi

#endif  // COMPONENTS_REQUEST_FILTER_FILTERED_REQUEST_INFO_H_
