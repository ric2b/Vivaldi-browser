// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/android/data_sharing_conversion_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/data_sharing/internal/jni_headers/DataSharingConversionBridge_jni.h"
#include "components/data_sharing/public/jni_headers/GroupData_jni.h"
#include "components/data_sharing/public/jni_headers/GroupMember_jni.h"
#include "components/data_sharing/public/jni_headers/GroupToken_jni.h"
#include "components/data_sharing/public/jni_headers/ServiceStatus_jni.h"
#include "components/data_sharing/public/jni_headers/SharedEntity_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using base::android::ToTypedJavaArrayOfObjects;

namespace data_sharing {

// static
ScopedJavaLocalRef<jobject> DataSharingConversionBridge::CreateJavaGroupMember(
    JNIEnv* env,
    const GroupMember& member) {
  return Java_GroupMember_createGroupMember(
      env, ConvertUTF8ToJavaString(env, member.gaia_id),
      ConvertUTF8ToJavaString(env, member.display_name),
      ConvertUTF8ToJavaString(env, member.email), static_cast<int>(member.role),
      url::GURLAndroid::FromNativeGURL(env, member.avatar_url));
}

// static
ScopedJavaLocalRef<jobject> DataSharingConversionBridge::CreateJavaGroupToken(
    JNIEnv* env,
    const GroupToken& token) {
  return Java_GroupToken_createGroupToken(
      env, ConvertUTF8ToJavaString(env, token.group_id.value()),
      ConvertUTF8ToJavaString(env, token.access_token));
}

// static
ScopedJavaLocalRef<jobject> DataSharingConversionBridge::CreateJavaGroupData(
    JNIEnv* env,
    const GroupData& group_data) {
  std::vector<ScopedJavaLocalRef<jobject>> j_members;
  j_members.reserve(group_data.members.size());
  for (const GroupMember& member : group_data.members) {
    j_members.push_back(CreateJavaGroupMember(env, member));
  }
  return Java_GroupData_createGroupData(
      env,
      ConvertUTF8ToJavaString(env, group_data.group_token.group_id.value()),
      ConvertUTF8ToJavaString(env, group_data.display_name),
      ToTypedJavaArrayOfObjects(
          env, base::make_span(j_members),
          org_chromium_components_data_1sharing_GroupMember_clazz(env)),
      ConvertUTF8ToJavaString(env, group_data.group_token.access_token));
}

// static
ScopedJavaLocalRef<jobject>
DataSharingConversionBridge::CreateJavaServiceStatus(
    JNIEnv* env,
    const ServiceStatus& status) {
  return Java_ServiceStatus_createServiceStatus(
      env, static_cast<int>(status.signin_status),
      static_cast<int>(status.sync_status),
      static_cast<int>(status.collaboration_status));
}

// static
ScopedJavaLocalRef<jobject> DataSharingConversionBridge::CreateJavaSharedEntity(
    JNIEnv* env,
    const SharedEntity& entity) {
  int size = entity.specifics.ByteSize();
  std::vector<uint8_t> data(size);
  entity.specifics.SerializeToArray(data.data(), size);
  return Java_SharedEntity_createSharedEntity(
      env, ConvertUTF8ToJavaString(env, entity.group_id.value()),
      ConvertUTF8ToJavaString(env, entity.name), entity.version,
      entity.update_time.InMillisecondsSinceUnixEpoch(),
      entity.create_time.InMillisecondsSinceUnixEpoch(),
      ConvertUTF8ToJavaString(env, entity.client_tag_hash),
      base::android::ToJavaByteArray(env, data));
}

// static
ScopedJavaLocalRef<jobject>
DataSharingConversionBridge::CreateGroupDataOrFailureOutcome(
    JNIEnv* env,
    const DataSharingService::GroupDataOrFailureOutcome& data) {
  ScopedJavaLocalRef<jobject> j_group_data;
  DataSharingService::PeopleGroupActionFailure failure =
      DataSharingService::PeopleGroupActionFailure::kUnknown;
  if (data.has_value()) {
    j_group_data = CreateJavaGroupData(env, data.value());
  } else {
    failure = data.error();
  }
  return Java_DataSharingConversionBridge_createGroupDataOrFailureOutcome(
      env, j_group_data, static_cast<int>(failure));
}

// static
ScopedJavaLocalRef<jobject>
DataSharingConversionBridge::CreateGroupDataSetOrFailureOutcome(
    JNIEnv* env,
    const DataSharingService::GroupsDataSetOrFailureOutcome& data) {
  std::vector<ScopedJavaLocalRef<jobject>> j_groups_data;
  DataSharingService::PeopleGroupActionFailure failure =
      DataSharingService::PeopleGroupActionFailure::kUnknown;
  if (data.has_value()) {
    for (const GroupData& group : data.value()) {
      j_groups_data.push_back(CreateJavaGroupData(env, group));
    }
  } else {
    failure = data.error();
  }
  ScopedJavaLocalRef<jobjectArray> j_group_array;
  if (!j_groups_data.empty()) {
    j_group_array = ToTypedJavaArrayOfObjects(
        env, base::make_span(j_groups_data),
        org_chromium_components_data_1sharing_GroupData_clazz(env));
  }
  return Java_DataSharingConversionBridge_createGroupDataSetOrFailureOutcome(
      env, j_group_array, static_cast<int>(failure));
}

// static
ScopedJavaLocalRef<jobject>
DataSharingConversionBridge::CreatePeopleGroupActionOutcome(JNIEnv* env,
                                                            int value) {
  return Java_DataSharingConversionBridge_createPeopleGroupActionOutcome(
      AttachCurrentThread(), value);
}

// static
ScopedJavaLocalRef<jobject> DataSharingConversionBridge::CreateParseURLResult(
    JNIEnv* env,
    const DataSharingService::ParseURLResult& data) {
  ScopedJavaLocalRef<jobject> j_group_data;
  DataSharingService::ParseURLStatus status =
      DataSharingService::ParseURLStatus::kUnknown;
  if (data.has_value()) {
    j_group_data = CreateJavaGroupToken(env, data.value());
    status = DataSharingService::ParseURLStatus::kSuccess;
  } else {
    status = data.error();
  }
  return Java_DataSharingConversionBridge_createParseURLResult(
      env, j_group_data, static_cast<int>(status));
}

// static
ScopedJavaLocalRef<jobject>
DataSharingConversionBridge::CreateSharedDataPreviewOrFailureOutcome(
    JNIEnv* env,
    const DataSharingService::SharedDataPreviewOrFailureOutcome& data) {
  std::vector<ScopedJavaLocalRef<jobject>> j_entities;
  DataSharingService::PeopleGroupActionFailure failure =
      DataSharingService::PeopleGroupActionFailure::kUnknown;
  if (data.has_value()) {
    for (const SharedEntity& entity : data.value().shared_entities) {
      j_entities.push_back(CreateJavaSharedEntity(env, entity));
    }
  } else {
    failure = data.error();
  }
  ScopedJavaLocalRef<jobjectArray> j_entities_array;
  if (!j_entities.empty()) {
    j_entities_array = ToTypedJavaArrayOfObjects(
        env, base::make_span(j_entities),
        org_chromium_components_data_1sharing_SharedEntity_clazz(env));
  }
  return Java_DataSharingConversionBridge_createSharedDataPreviewOrFailureOutcome(
      env, j_entities_array, static_cast<int>(failure));
}
}  // namespace data_sharing
