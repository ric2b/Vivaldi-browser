// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/channel/namespace_router.h"

#include <utility>

#include "cast/common/channel/proto/cast_channel.pb.h"

namespace openscreen::cast {

NamespaceRouter::NamespaceRouter() = default;
NamespaceRouter::~NamespaceRouter() = default;

void NamespaceRouter::AddNamespaceHandler(std::string namespace_,
                                          CastMessageHandler* handler) {
  handlers_.emplace(std::move(namespace_), handler);
}

void NamespaceRouter::RemoveNamespaceHandler(const std::string& namespace_) {
  handlers_.erase(namespace_);
}

void NamespaceRouter::OnMessage(VirtualConnectionRouter* router,
                                CastSocket* socket,
                                proto::CastMessage message) {
  const std::string& ns = message.namespace_();
  auto it = handlers_.find(ns);
  if (it != handlers_.end()) {
    it->second->OnMessage(router, socket, std::move(message));
  }
}

}  // namespace openscreen::cast
