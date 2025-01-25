// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/channel/message_util.h"

#include <sstream>
#include <utility>

#include "build/build_config.h"
#include "cast/common/channel/virtual_connection.h"
#include "util/enum_name_table.h"
#include "util/json/json_serialization.h"
#include "util/json/json_value.h"
#include "util/osp_logging.h"

#if BUILDFLAG(IS_APPLE)
#include <TargetConditionals.h>
#endif

namespace openscreen::cast {
namespace {

using proto::CastMessage;

// The value used for "sdkType" in a virtual CONNECT request. Historically, this
// value was used in Chrome's C++ impl even though "2" refers to the Media
// Router Extension.
constexpr int kVirtualConnectSdkType = 2;

// The value used for "connectionType" in a virtual CONNECT request. This value
// stands for CONNECTION_TYPE_LOCAL.
constexpr int kVirtualConnectTypeLocal = 1;

// The value to be set as the "platform" value in a virtual CONNECT request.
// Source (in Chromium source tree):
// src/third_party/metrics_proto/cast_logs.proto
enum VirtualConnectPlatformValue {
  kOtherPlatform = 0,
  kAndroid = 1,
  kIOS = 2,
  kWindows = 3,
  kMacOSX = 4,
  kChromeOS = 5,
  kLinux = 6,
  kCastDevice = 7,
};

constexpr VirtualConnectPlatformValue GetVirtualConnectPlatform() {
  // Based on //build/build_config.h in the Chromium project. The order of these
  // matters!
#if BUILDFLAG(IS_ANDROID)
  return kAndroid;
#elif BUILDFLAG(IS_IOS)
  return kIOS;
#elif BUILDFLAG(IS_WIN)
  return kWindows;
#elif BUILDFLAG(IS_MAC)
  return kMacOSX;
#elif BUILDFLAG(IS_CHROMEOS)
  return kChromeOS;
#elif BUILDFLAG(IS_LINUX)
  return kLinux;
#else
  return kOtherPlatform;
#endif
}

CastMessage MakeConnectionMessage(const std::string& source_id,
                                  const std::string& destination_id) {
  CastMessage connect_message;
  connect_message.set_protocol_version(kDefaultOutgoingMessageVersion);
  connect_message.set_source_id(source_id);
  connect_message.set_destination_id(destination_id);
  connect_message.set_namespace_(kConnectionNamespace);
  return connect_message;
}

}  // namespace

std::string ToString(AppAvailabilityResult availability) {
  switch (availability) {
    case AppAvailabilityResult::kAvailable:
      return "Available";
    case AppAvailabilityResult::kUnavailable:
      return "Unavailable";
    case AppAvailabilityResult::kUnknown:
      return "Unknown";
    default:
      OSP_NOTREACHED();
  }
}

CastMessage MakeSimpleUTF8Message(const std::string& namespace_,
                                  std::string payload) {
  CastMessage message;
  message.set_protocol_version(kDefaultOutgoingMessageVersion);
  message.set_namespace_(namespace_);
  message.set_payload_type(proto::CastMessage_PayloadType_STRING);
  message.set_payload_utf8(std::move(payload));
  return message;
}

CastMessage MakeConnectMessage(const std::string& source_id,
                               const std::string& destination_id) {
  CastMessage connect_message =
      MakeConnectionMessage(source_id, destination_id);
  connect_message.set_payload_type(proto::CastMessage_PayloadType_STRING);

  // Historically, the CONNECT message was meant to come from a Chrome browser.
  // However, this library could be embedded in any app. So, properties like
  // user agent, application version, etc. are not known here.
  static constexpr char kUnknownVersion[] = "Unknown (Open Screen)";

  Json::Value message(Json::objectValue);
  message[kMessageKeyType] = CastMessageTypeToString(CastMessageType::kConnect);
  for (int i = 0; i <= 3; ++i) {
    message[kMessageKeyProtocolVersionList][i] =
        proto::CastMessage_ProtocolVersion_CASTV2_1_0 + i;
  }
  message[kMessageKeyUserAgent] = kUnknownVersion;
  message[kMessageKeyConnType] =
      static_cast<int>(VirtualConnection::Type::kStrong);
  message[kMessageKeyOrigin] = Json::Value(Json::objectValue);

  Json::Value sender_info(Json::objectValue);
  sender_info[kMessageKeySdkType] = kVirtualConnectSdkType;
  sender_info[kMessageKeyVersion] = kUnknownVersion;
  sender_info[kMessageKeyBrowserVersion] = kUnknownVersion;
  sender_info[kMessageKeyPlatform] = GetVirtualConnectPlatform();
  sender_info[kMessageKeyConnectionType] = kVirtualConnectTypeLocal;
  message[kMessageKeySenderInfo] = std::move(sender_info);

  connect_message.set_payload_utf8(json::Stringify(std::move(message)).value());
  return connect_message;
}

CastMessage MakeCloseMessage(const std::string& source_id,
                             const std::string& destination_id) {
  CastMessage close_message = MakeConnectionMessage(source_id, destination_id);
  close_message.set_payload_type(proto::CastMessage_PayloadType_STRING);
  close_message.set_payload_utf8(R"!({"type": "CLOSE"})!");
  return close_message;
}

std::string MakeUniqueSessionId(const char* prefix) {
  static int next_id = 10000;

  std::ostringstream oss;
  oss << prefix << '-' << (next_id++);
  return oss.str();
}

bool HasType(const Json::Value& object, CastMessageType type) {
  OSP_CHECK(object.isObject());
  const Json::Value& value =
      object.get(kMessageKeyType, Json::Value::nullSingleton());
  return value.isString() && value.asString() == CastMessageTypeToString(type);
}

std::string ToString(const CastMessage& message) {
  std::stringstream ss;
  ss << "CastMessage(source=" << message.source_id()
     << ", dest=" << message.destination_id()
     << ", namespace=" << message.namespace_()
     << ", payload_utf8=" << message.payload_utf8()
     << ", payload_binary=" << message.payload_binary()
     << ", remaining length=" << message.remaining_length() << ")";
  return ss.str();
}

constexpr EnumNameTable<CastMessageType, 25> kCastMessageTypeNames{
    {{"PING", CastMessageType::kPing},
     {"PONG", CastMessageType::kPong},
     {"RPC", CastMessageType::kRpc},
     {"GET_APP_AVAILABILITY", CastMessageType::kGetAppAvailability},
     {"GET_STATUS", CastMessageType::kGetStatus},
     {"CONNECT", CastMessageType::kConnect},
     {"CLOSE", CastMessageType::kCloseConnection},
     {"APPLICATION_BROADCAST", CastMessageType::kBroadcast},
     {"LAUNCH", CastMessageType::kLaunch},
     {"STOP", CastMessageType::kStop},
     {"RECEIVER_STATUS", CastMessageType::kReceiverStatus},
     {"MEDIA_STATUS", CastMessageType::kMediaStatus},
     {"LAUNCH_ERROR", CastMessageType::kLaunchError},
     {"OFFER", CastMessageType::kOffer},
     {"ANSWER", CastMessageType::kAnswer},
     {"CAPABILITIES_RESPONSE", CastMessageType::kCapabilitiesResponse},
     {"STATUS_RESPONSE", CastMessageType::kStatusResponse},
     {"MULTIZONE_STATUS", CastMessageType::kMultizoneStatus},
     {"INVALID_PLAYER_STATE", CastMessageType::kInvalidPlayerState},
     {"LOAD_FAILED", CastMessageType::kLoadFailed},
     {"LOAD_CANCELLED", CastMessageType::kLoadCancelled},
     {"INVALID_REQUEST", CastMessageType::kInvalidRequest},
     {"PRESENTATION", CastMessageType::kPresentation},
     {"GET_CAPABILITIES", CastMessageType::kGetCapabilities},
     {"OTHER", CastMessageType::kOther}}};

const char* CastMessageTypeToString(CastMessageType type) {
  return GetEnumName(kCastMessageTypeNames, type).value("OTHER");
}

const std::string& GetPayload(const CastMessage& message) {
  // Receiver messages will report if they are string or binary, but may
  // populate either the utf8 or the binary field with the message contents.
  // TODO(https://crbug.com/1429410): CastSocket's CastMessage results have
  // wrong payload field filled out.
  OSP_CHECK_EQ(message.payload_type(), proto::CastMessage::STRING);
  return !message.payload_utf8().empty() ? message.payload_utf8()
                                         : message.payload_binary();
}

}  // namespace openscreen::cast
