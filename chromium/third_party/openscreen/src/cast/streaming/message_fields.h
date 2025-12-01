// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_MESSAGE_FIELDS_H_
#define CAST_STREAMING_MESSAGE_FIELDS_H_

#include <string>
#include <string_view>

#include "cast/streaming/public/constants.h"
#include "platform/base/error.h"

namespace openscreen::cast {

/// NOTE: Constants here are all taken from the Cast V2: Mirroring Control
/// Protocol specification.

// Namespace for OFFER/ANSWER messages.
inline constexpr char kCastWebrtcNamespace[] =
    "urn:x-cast:com.google.cast.webrtc";
inline constexpr char kCastRemotingNamespace[] =
    "urn:x-cast:com.google.cast.remoting";

// JSON message field values specific to the Sender Session.
inline constexpr char kMessageType[] = "type";

// List of OFFER message fields.
inline constexpr char kMessageTypeOffer[] = "OFFER";
inline constexpr char kOfferMessageBody[] = "offer";
inline constexpr char kSequenceNumber[] = "seqNum";
inline constexpr char kCodecName[] = "codecName";

/// ANSWER message fields.
inline constexpr char kMessageTypeAnswer[] = "ANSWER";
inline constexpr char kAnswerMessageBody[] = "answer";
inline constexpr char kResult[] = "result";
inline constexpr char kResultOk[] = "ok";
inline constexpr char kResultError[] = "error";
inline constexpr char kErrorMessageBody[] = "error";
inline constexpr char kErrorCode[] = "code";
inline constexpr char kErrorDescription[] = "description";

// Other message fields.
inline constexpr char kRpcMessageBody[] = "rpc";
inline constexpr char kCapabilitiesMessageBody[] = "capabilities";
inline constexpr char kStatusMessageBody[] = "status";

// Conversion methods for codec message fields.
const char* CodecToString(AudioCodec codec);
ErrorOr<AudioCodec> StringToAudioCodec(std::string_view name);

const char* CodecToString(VideoCodec codec);
ErrorOr<VideoCodec> StringToVideoCodec(std::string_view name);

}  // namespace openscreen::cast

#endif  // CAST_STREAMING_MESSAGE_FIELDS_H_
