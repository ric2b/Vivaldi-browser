// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CHANNEL_MESSAGE_UTIL_H_
#define CAST_COMMON_CHANNEL_MESSAGE_UTIL_H_

#include <string>
#include <string_view>

#include "cast/common/channel/proto/cast_channel.pb.h"

namespace Json {
class Value;
}

namespace openscreen::cast {

// Reserved message namespaces for internal messages.
static constexpr char kCastInternalNamespacePrefix[] =
    "urn:x-cast:com.google.cast.";
static constexpr char kTransportNamespacePrefix[] =
    "urn:x-cast:com.google.cast.tp.";
static constexpr char kAuthNamespace[] =
    "urn:x-cast:com.google.cast.tp.deviceauth";
static constexpr char kHeartbeatNamespace[] =
    "urn:x-cast:com.google.cast.tp.heartbeat";
static constexpr char kConnectionNamespace[] =
    "urn:x-cast:com.google.cast.tp.connection";
static constexpr char kReceiverNamespace[] =
    "urn:x-cast:com.google.cast.receiver";
static constexpr char kBroadcastNamespace[] =
    "urn:x-cast:com.google.cast.broadcast";
static constexpr char kMediaNamespace[] = "urn:x-cast:com.google.cast.media";

// Sender and receiver IDs to use for platform messages.
static constexpr char kPlatformSenderId[] = "sender-0";
static constexpr char kPlatformReceiverId[] = "receiver-0";

static constexpr char kBroadcastId[] = "*";

static constexpr proto::CastMessage_ProtocolVersion
    kDefaultOutgoingMessageVersion =
        proto::CastMessage_ProtocolVersion_CASTV2_1_0;

// JSON message key strings.
static constexpr char kMessageKeyType[] = "type";
static constexpr char kMessageKeyProtocolVersion[] = "protocolVersion";
static constexpr char kMessageKeyProtocolVersionList[] = "protocolVersionList";
static constexpr char kMessageKeyReasonCode[] = "reasonCode";
static constexpr char kMessageKeyAppId[] = "appId";
static constexpr char kMessageKeyRequestId[] = "requestId";
static constexpr char kMessageKeyResponseType[] = "responseType";
static constexpr char kMessageKeyTransportId[] = "transportId";
static constexpr char kMessageKeySessionId[] = "sessionId";

// JSON message field values.
static constexpr char kMessageTypeConnect[] = "CONNECT";
static constexpr char kMessageTypeClose[] = "CLOSE";
static constexpr char kMessageTypeConnected[] = "CONNECTED";
static constexpr char kMessageValueAppAvailable[] = "APP_AVAILABLE";
static constexpr char kMessageValueAppUnavailable[] = "APP_UNAVAILABLE";

// JSON message key strings specific to CONNECT messages.
static constexpr char kMessageKeyBrowserVersion[] = "browserVersion";
static constexpr char kMessageKeyConnType[] = "connType";
static constexpr char kMessageKeyConnectionType[] = "connectionType";
static constexpr char kMessageKeyUserAgent[] = "userAgent";
static constexpr char kMessageKeyOrigin[] = "origin";
static constexpr char kMessageKeyPlatform[] = "platform";
static constexpr char kMessageKeySdkType[] = "skdType";
static constexpr char kMessageKeySenderInfo[] = "senderInfo";
static constexpr char kMessageKeyVersion[] = "version";

// JSON message key strings specific to application control messages.
static constexpr char kMessageKeyAvailability[] = "availability";
static constexpr char kMessageKeyAppParams[] = "appParams";
static constexpr char kMessageKeyApplications[] = "applications";
static constexpr char kMessageKeyControlType[] = "controlType";
static constexpr char kMessageKeyDisplayName[] = "displayName";
static constexpr char kMessageKeyIsIdleScreen[] = "isIdleScreen";
static constexpr char kMessageKeyLaunchedFromCloud[] = "launchedFromCloud";
static constexpr char kMessageKeyLevel[] = "level";
static constexpr char kMessageKeyMuted[] = "muted";
static constexpr char kMessageKeyName[] = "name";
static constexpr char kMessageKeyNamespaces[] = "namespaces";
static constexpr char kMessageKeyReason[] = "reason";
static constexpr char kMessageKeyStatus[] = "status";
static constexpr char kMessageKeyStepInterval[] = "stepInterval";
static constexpr char kMessageKeyUniversalAppId[] = "universalAppId";
static constexpr char kMessageKeyUserEq[] = "userEq";
static constexpr char kMessageKeyVolume[] = "volume";

// JSON message field value strings specific to application control messages.
static constexpr char kMessageValueAttenuation[] = "attenuation";
static constexpr char kMessageValueBadParameter[] = "BAD_PARAMETER";
static constexpr char kMessageValueInvalidSessionId[] = "INVALID_SESSION_ID";
static constexpr char kMessageValueInvalidCommand[] = "INVALID_COMMAND";
static constexpr char kMessageValueNotFound[] = "NOT_FOUND";
static constexpr char kMessageValueSystemError[] = "SYSTEM_ERROR";

enum class CastMessageType {
  // Heartbeat messages.
  kPing,
  kPong,

  // RPC control/status messages used by Media Remoting. These occur at high
  // frequency, up to dozens per second at times, and should not be logged.
  kRpc,

  kGetAppAvailability,
  kGetStatus,

  // Virtual connection request.
  kConnect,

  // Close virtual connection.
  kCloseConnection,

  // Application broadcast / precache.
  kBroadcast,

  // Session launch request.
  kLaunch,

  // Session stop request.
  kStop,

  kReceiverStatus,
  kMediaStatus,

  // Error from receiver.
  kLaunchError,

  kOffer,
  kAnswer,
  kCapabilitiesResponse,
  kStatusResponse,

  // The following values are part of the protocol but are not currently used.
  kMultizoneStatus,
  kInvalidPlayerState,
  kLoadFailed,
  kLoadCancelled,
  kInvalidRequest,
  kPresentation,
  kGetCapabilities,

  kOther,  // Add new types above `kOther`.
  kMaxValue = kOther,
};

enum class AppAvailabilityResult {
  kAvailable,
  kUnavailable,
  kUnknown,
};

std::string ToString(AppAvailabilityResult availability);

const char* CastMessageTypeToString(CastMessageType type);

inline bool IsAuthMessage(const proto::CastMessage& message) {
  return message.namespace_() == kAuthNamespace;
}

inline bool IsTransportNamespace(std::string_view namespace_) {
  return (namespace_.size() > (sizeof(kTransportNamespacePrefix) - 1)) &&
         (namespace_.find_first_of(kTransportNamespacePrefix) == 0);
}

proto::CastMessage MakeSimpleUTF8Message(const std::string& namespace_,
                                         std::string payload);

proto::CastMessage MakeConnectMessage(const std::string& source_id,
                                      const std::string& destination_id);
proto::CastMessage MakeCloseMessage(const std::string& source_id,
                                    const std::string& destination_id);

// Returns a session/transport ID string that is unique within this application
// instance, having the format "prefix-12345". For example, calling this with a
// `prefix` of "sender" will result in a string like "sender-12345".
std::string MakeUniqueSessionId(const char* prefix);

// Returns true if the type field in `object` is set to the given `type`.
bool HasType(const Json::Value& object, CastMessageType type);

// Serializes a given cast message to a string.
std::string ToString(const proto::CastMessage& message);

// Helper to get the actual message payload out of a cast message.
const std::string& GetPayload(const proto::CastMessage& message);

}  // namespace openscreen::cast

#endif  // CAST_COMMON_CHANNEL_MESSAGE_UTIL_H_
