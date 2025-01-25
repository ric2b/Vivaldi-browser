// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CHANNEL_NAMESPACE_ROUTER_H_
#define CAST_COMMON_CHANNEL_NAMESPACE_ROUTER_H_

#include <map>
#include <string>

#include "cast/common/channel/cast_message_handler.h"
#include "cast/common/channel/proto/cast_channel.pb.h"

namespace openscreen::cast {

class NamespaceRouter final : public CastMessageHandler {
 public:
  NamespaceRouter();
  ~NamespaceRouter() override;

  void AddNamespaceHandler(std::string namespace_, CastMessageHandler* handler);
  void RemoveNamespaceHandler(const std::string& namespace_);

  // CastMessageHandler overrides.
  void OnMessage(VirtualConnectionRouter* router,
                 CastSocket* socket,
                 proto::CastMessage message) override;

 private:
  std::map<std::string /* namespace */, CastMessageHandler*> handlers_;
};

}  // namespace openscreen::cast

#endif  // CAST_COMMON_CHANNEL_NAMESPACE_ROUTER_H_
