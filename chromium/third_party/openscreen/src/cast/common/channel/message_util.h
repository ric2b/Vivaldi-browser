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
inline constexpr char kCastInternalNamespacePrefix[] =
    "urn:x-cast:com.google.cast.";
inline constexpr char kTransportNamespacePrefix[] =
    "urn:x-cast:com.google.cast.tp.";
inline constexpr char kAuthNamespace[] =
    "urn:x-cast:com.google.cast.tp.deviceauth";
inline constexpr char kHeartbeatNamespace[] =
    "urn:x-cast:com.google.cast.tp.heartbeat";
inline constexpr char kConnectionNamespace[] =
    "urn:x-cast:com.google.cast.tp.connection";
inline constexpr char kReceiverNamespace[] =
    "urn:x-cast:com.google.cast.receiver";
inline constexpr char kBroadcastNamespace[] =
    "urn:x-cast:com.google.cast.broadcast";
inline constexpr char kMediaNamespace[] = "urn:x-cast:com.google.cast.media";

// Sender and receiver IDs to use for platform messages.
inline constexpr char kPlatformSenderId[] = "sender-0";
inline constexpr char kPlatformReceiverId[] = "receiver-0";

inline constexpr char kBroadcastId[] = "*";

inline constexpr proto::CastMessage_ProtocolVersion
    kDefaultOutgoingMessageVersion =
        proto::CastMessage_ProtocolVersion_CASTV2_1_0;

// JSON message key strings.
inline constexpr char kMessageKeyType[] = "type";
inline constexpr char kMessageKeyProtocolVersion[] = "protocolVersion";
inline constexpr char kMessageKeyProtocolVersionList[] = "protocolVersionList";
inline constexpr char kMessageKeyReasonCode[] = "reasonCode";
inline constexpr char kMessageKeyAppId[] = "appId";
inline constexpr char kMessageKeyRequestId[] = "requestId";
inline constexpr char kMessageKeyResponseType[] = "responseType";
inline constexpr char kMessageKeyTransportId[] = "transportId";
inline constexpr char kMessageKeySessionId[] = "sessionId";

// JSON message field values.
inline constexpr char kMessageTypeConnect[] = "CONNECT";
inline constexpr char kMessageTypeClose[] = "CLOSE";
inline constexpr char kMessageTypeConnected[] = "CONNECTED";
inline constexpr char kMessageValueAppAvailable[] = "APP_AVAILABLE";
inline constexpr char kMessageValueAppUnavailable[] = "APP_UNAVAILABLE";

// JSON message key strings specific to CONNECT messages.
inline constexpr char kMessageKeyBrowserVersion[] = "browserVersion";
inline constexpr char kMessageKeyConnType[] = "connType";
inline constexpr char kMessageKeyConnectionType[] = "connectionType";
inline constexpr char kMessageKeyUserAgent[] = "userAgent";
inline constexpr char kMessageKeyOrigin[] = "origin";
inline constexpr char kMessageKeyPlatform[] = "platform";
inline constexpr char kMessageKeySdkType[] = "skdType";
inline constexpr char kMessageKeySenderInfo[] = "senderInfo";
inline constexpr char kMessageKeyVersion[] = "version";

// JSON message key strings specific to application control messages.
inline constexpr char kMessageKeyAvailability[] = "availability";
inline constexpr char kMessageKeyAppParams[] = "appParams";
inline constexpr char kMessageKeyApplications[] = "applications";
inline constexpr char kMessageKeyControlType[] = "controlType";
inline constexpr char kMessageKeyDisplayName[] = "displayName";
inline constexpr char kMessageKeyIsIdleScreen[] = "isIdleScreen";
inline constexpr char kMessageKeyLaunchedFromCloud[] = "launchedFromCloud";
inline constexpr char kMessageKeyLevel[] = "level";
inline constexpr char kMessageKeyMuted[] = "muted";
inline constexpr char kMessageKeyName[] = "name";
inline constexpr char kMessageKeyNamespaces[] = "namespaces";
inline constexpr char kMessageKeyReason[] = "reason";
inline constexpr char kMessageKeyStatus[] = "status";
inline constexpr char kMessageKeyStepInterval[] = "stepInterval";
inline constexpr char kMessageKeyUniversalAppId[] = "universalAppId";
inline constexpr char kMessageKeyUserEq[] = "userEq";
inline constexpr char kMessageKeyVolume[] = "volume";

// JSON message field value strings specific to application control messages.
inline constexpr char kMessageValueAttenuation[] = "attenuation";
inline constexpr char kMessageValueBadParameter[] = "BAD_PARAMETER";
inline constexpr char kMessageValueInvalidSessionId[] = "INVALID_SESSION_ID";
inline constexpr char kMessageValueInvalidCommand[] = "INVALID_COMMAND";
inline constexpr char kMessageValueNotFound[] = "NOT_FOUND";
inline constexpr char kMessageValueSystemError[] = "SYSTEM_ERROR";

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
