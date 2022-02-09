// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_REQUEST_FILTER_PROXYING_WEBTRANSPORT_H_
#define COMPONENTS_REQUEST_FILTER_REQUEST_FILTER_PROXYING_WEBTRANSPORT_H_

#include "components/request_filter/request_filter_manager.h"
#include "content/public/browser/content_browser_client.h"

class GURL;

namespace vivaldi {

// Starts proxying WebTransport handshake if the extensions want to listen it
// by overrinding `handshake_client`.
void StartWebRequestProxyingWebTransport(
    content::RenderProcessHost& render_process_host,
    int frame_routing_id,
    const GURL& url,
    const url::Origin& initiator_origin,
    mojo::PendingRemote<network::mojom::WebTransportHandshakeClient>
        handshake_client,
    int64_t request_id,
    RequestFilterManager::RequestHandler* request_handler,
    RequestFilterManager::ProxySet& proxies,
    content::ContentBrowserClient::WillCreateWebTransportCallback callback);

}  // namespace vivaldi

#endif  // COMPONENTS_REQUEST_FILTER_REQUEST_FILTER_PROXYING_WEBTRANSPORT_H_