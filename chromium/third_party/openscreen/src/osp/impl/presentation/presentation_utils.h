// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_PRESENTATION_PRESENTATION_UTILS_H_
#define OSP_IMPL_PRESENTATION_PRESENTATION_UTILS_H_

#include <memory>

#include "osp/msgs/osp_messages.h"
#include "osp/public/message_demuxer.h"
#include "osp/public/presentation/presentation_connection.h"

namespace openscreen::osp {

// These methods retrieve the server and client demuxers, respectively. These
// are retrieved from the protocol connection server and client.
MessageDemuxer& GetServerDemuxer();
MessageDemuxer& GetClientDemuxer();

// These methods try to create a ProtocolConnection for server and client,
// respectively. Nullptr is returned on failure, so check the return value
// before using it.
std::unique_ptr<ProtocolConnection> CreateServerProtocolConnection(
    uint64_t instance_id);
std::unique_ptr<ProtocolConnection> CreateClientProtocolConnection(
    uint64_t instance_id);

msgs::PresentationConnectionCloseEvent_reason ConvertCloseEventReason(
    Connection::CloseReason reason);

msgs::PresentationTerminationSource ConvertTerminationSource(
    TerminationSource source);

msgs::PresentationTerminationReason ConvertTerminationReason(
    TerminationReason reason);

}  // namespace openscreen::osp

#endif  // OSP_IMPL_PRESENTATION_PRESENTATION_UTILS_H_
