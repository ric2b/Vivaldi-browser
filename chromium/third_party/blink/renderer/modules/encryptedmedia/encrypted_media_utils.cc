// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/encryptedmedia/encrypted_media_utils.h"

#include "base/notreached.h"
#include "media/base/eme_constants.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"

namespace blink {

namespace {

const char kTemporary[] = "temporary";
const char kPersistentLicense[] = "persistent-license";

}  // namespace

media::EmeInitDataType EncryptedMediaUtils::ConvertToInitDataType(
    const String& init_data_type) {
  if (init_data_type == "cenc")
    return media::EmeInitDataType::CENC;
  if (init_data_type == "keyids")
    return media::EmeInitDataType::KEYIDS;
  if (init_data_type == "webm")
    return media::EmeInitDataType::WEBM;

  // |initDataType| is not restricted in the idl, so anything is possible.
  return media::EmeInitDataType::UNKNOWN;
}

String EncryptedMediaUtils::ConvertFromInitDataType(
    media::EmeInitDataType init_data_type) {
  switch (init_data_type) {
    case media::EmeInitDataType::CENC:
      return "cenc";
    case media::EmeInitDataType::KEYIDS:
      return "keyids";
    case media::EmeInitDataType::WEBM:
      return "webm";
    case media::EmeInitDataType::UNKNOWN:
      // Chromium should not use Unknown, but we use it in Blink when the
      // actual value has been blocked for non-same-origin or mixed content.
      return String();
  }

  NOTREACHED_IN_MIGRATION();
  return String();
}

// static
WebEncryptedMediaSessionType EncryptedMediaUtils::ConvertToSessionType(
    const String& session_type) {
  if (session_type == kTemporary)
    return WebEncryptedMediaSessionType::kTemporary;
  if (session_type == kPersistentLicense)
    return WebEncryptedMediaSessionType::kPersistentLicense;

  // |sessionType| is not restricted in the idl, so anything is possible.
  return WebEncryptedMediaSessionType::kUnknown;
}

// static
String EncryptedMediaUtils::ConvertFromSessionType(
    WebEncryptedMediaSessionType session_type) {
  switch (session_type) {
    case WebEncryptedMediaSessionType::kTemporary:
      return kTemporary;
    case WebEncryptedMediaSessionType::kPersistentLicense:
      return kPersistentLicense;
    case WebEncryptedMediaSessionType::kUnknown:
      // Unexpected session type from Chromium.
      NOTREACHED_IN_MIGRATION();
      return String();
  }

  NOTREACHED_IN_MIGRATION();
  return String();
}

// static
String EncryptedMediaUtils::ConvertKeyStatusToString(
    const WebEncryptedMediaKeyInformation::KeyStatus status) {
  switch (status) {
    case WebEncryptedMediaKeyInformation::KeyStatus::kUsable:
      return "usable";
    case WebEncryptedMediaKeyInformation::KeyStatus::kExpired:
      return "expired";
    case WebEncryptedMediaKeyInformation::KeyStatus::kReleased:
      return "released";
    case WebEncryptedMediaKeyInformation::KeyStatus::kOutputRestricted:
      return "output-restricted";
    case WebEncryptedMediaKeyInformation::KeyStatus::kOutputDownscaled:
      return "output-downscaled";
    case WebEncryptedMediaKeyInformation::KeyStatus::kStatusPending:
      return "status-pending";
    case WebEncryptedMediaKeyInformation::KeyStatus::kInternalError:
      return "internal-error";
  }

  NOTREACHED_IN_MIGRATION();
  return "internal-error";
}

// static
V8MediaKeyStatus EncryptedMediaUtils::ConvertKeyStatusToEnum(
    const WebEncryptedMediaKeyInformation::KeyStatus status) {
  switch (status) {
    case WebEncryptedMediaKeyInformation::KeyStatus::kUsable:
      return V8MediaKeyStatus(V8MediaKeyStatus::Enum::kUsable);
    case WebEncryptedMediaKeyInformation::KeyStatus::kExpired:
      return V8MediaKeyStatus(V8MediaKeyStatus::Enum::kExpired);
    case WebEncryptedMediaKeyInformation::KeyStatus::kReleased:
      return V8MediaKeyStatus(V8MediaKeyStatus::Enum::kReleased);
    case WebEncryptedMediaKeyInformation::KeyStatus::kOutputRestricted:
      return V8MediaKeyStatus(V8MediaKeyStatus::Enum::kOutputRestricted);
    case WebEncryptedMediaKeyInformation::KeyStatus::kOutputDownscaled:
      return V8MediaKeyStatus(V8MediaKeyStatus::Enum::kOutputDownscaled);
    case WebEncryptedMediaKeyInformation::KeyStatus::kStatusPending:
      return V8MediaKeyStatus(V8MediaKeyStatus::Enum::kStatusPending);
    case WebEncryptedMediaKeyInformation::KeyStatus::kInternalError:
      return V8MediaKeyStatus(V8MediaKeyStatus::Enum::kInternalError);
  }
}

// static
WebMediaKeySystemConfiguration::Requirement
EncryptedMediaUtils::ConvertToMediaKeysRequirement(const String& requirement) {
  if (requirement == "required")
    return WebMediaKeySystemConfiguration::Requirement::kRequired;
  if (requirement == "optional")
    return WebMediaKeySystemConfiguration::Requirement::kOptional;
  if (requirement == "not-allowed")
    return WebMediaKeySystemConfiguration::Requirement::kNotAllowed;

  NOTREACHED_IN_MIGRATION();
  return WebMediaKeySystemConfiguration::Requirement::kOptional;
}

// static
String EncryptedMediaUtils::ConvertMediaKeysRequirementToString(
    WebMediaKeySystemConfiguration::Requirement requirement) {
  switch (requirement) {
    case WebMediaKeySystemConfiguration::Requirement::kRequired:
      return "required";
    case WebMediaKeySystemConfiguration::Requirement::kOptional:
      return "optional";
    case WebMediaKeySystemConfiguration::Requirement::kNotAllowed:
      return "not-allowed";
  }

  NOTREACHED_IN_MIGRATION();
  return "not-allowed";
}

WebEncryptedMediaClient*
EncryptedMediaUtils::GetEncryptedMediaClientFromLocalDOMWindow(
    LocalDOMWindow* window) {
  WebLocalFrameImpl* web_frame =
      WebLocalFrameImpl::FromFrame(window->GetFrame());
  return web_frame->Client()->EncryptedMediaClient();
}

}  // namespace blink
