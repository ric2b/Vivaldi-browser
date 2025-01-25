// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved
#include "direct_match_bridge.h"

#include <stddef.h>
#include <stdint.h>
#include <memory>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"

#include "chrome/browser/profiles/profile.h"
#include "components/direct_match/direct_match_service.h"
#include "components/direct_match/direct_match_service_factory.h"
#include "content/public/browser/browser_thread.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/stack.h"

#include "chrome/android/chrome_jni_headers/DirectMatchBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaIntArray;
using content::BrowserThread;
using direct_match::DirectMatchService;
using direct_match::DirectMatchServiceFactory;

DirectMatchBridge::DirectMatchBridge(JNIEnv* env,
                                     const JavaRef<jobject>& obj,
                                     const JavaRef<jobject>& j_profile)
    : profile_(Profile::FromJavaObject(j_profile)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  directmatch_service =
      direct_match::DirectMatchServiceFactory::GetForBrowserContext(profile_);
  directmatch_service->AddObserver(this);
}

DirectMatchBridge::~DirectMatchBridge() {
}

void DirectMatchBridge::Destroy(JNIEnv*, const JavaParamRef<jobject>&) {
  directmatch_service->RemoveObserver(this);
  delete this;
}

void DirectMatchBridge::AddItemsToDirectMatchItemList(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_result_obj,
    const std::vector<const DirectMatchService::DirectMatchUnit*>& nodes) {
  for (const auto* unit : nodes) {
    Java_DirectMatchBridge_addToDirectMatchItemList(
        env, j_result_obj,
        Java_DirectMatchBridge_createDirectMatchItem(
            env, ConvertUTF8ToJavaString(env, unit->name),
            ConvertUTF8ToJavaString(env, unit->title),
            ConvertUTF8ToJavaString(env, unit->redirect_url),
            ConvertUTF8ToJavaString(env, unit->image_url), unit->match_offset,
            ConvertUTF8ToJavaString(env, unit->image_path), unit->category,
            unit->position));
  }
}

void DirectMatchBridge::GetPopularSites(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_result_obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (directmatch_service != NULL) {
    std::vector<const DirectMatchService::DirectMatchUnit*> direct_match_items =
        directmatch_service->GetPopularSites();
    AddItemsToDirectMatchItemList(env, j_result_obj, direct_match_items);
  }
}

void DirectMatchBridge::GetDirectMatchesForCategory(
    JNIEnv* env,
    int category_id,
    const base::android::JavaParamRef<jobject>& j_result_obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AddItemsToDirectMatchItemList(env, j_result_obj,
                                GetDirectMatchItemList(category_id));
}

const std::vector<const DirectMatchService::DirectMatchUnit*>
DirectMatchBridge::GetDirectMatchItemList(int category) {
  std::vector<const DirectMatchService::DirectMatchUnit*> direct_match_items;
  direct_match_items =
      directmatch_service->GetDirectMatchesForCategory(category);
  return direct_match_items;
}

void DirectMatchBridge::DirectMatchUnitsDownloaded() {
  JNIEnv* env = AttachCurrentThread();
  jni_zero::ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_DirectMatchBridge_directMatchUnitsDownloadFinished(env, obj);
}

void DirectMatchBridge::DirectMatchIconsDownloaded() {
  JNIEnv* env = AttachCurrentThread();
  jni_zero::ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_DirectMatchBridge_directMatchIconsDownloadFinished(env, obj);
}

ScopedJavaLocalRef<jobject> DirectMatchBridge::CreateJavaDirectMatchItem(
    const direct_match::DirectMatchService::DirectMatchUnit* unit) {
  JNIEnv* env = AttachCurrentThread();

  return Java_DirectMatchBridge_createDirectMatchItem(
      env, /*uuid*/ ConvertUTF8ToJavaString(env, unit->title),
      ConvertUTF8ToJavaString(env, unit->title),
      ConvertUTF8ToJavaString(env, unit->title),
      ConvertUTF8ToJavaString(env, unit->redirect_url),
      ConvertUTF8ToJavaString(env, unit->image_url), 1,
      ConvertUTF8ToJavaString(env, unit->image_path), 1, 1);
}

static jlong JNI_DirectMatchBridge_Init(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& caller,
    const jni_zero::JavaParamRef<jobject>& profile) {
  DirectMatchBridge* delegate = new DirectMatchBridge(env, caller, profile);
  return reinterpret_cast<intptr_t>(delegate);
}

void DirectMatchBridge::OnFinishedDownloadingDirectMatchUnits() {
  JNIEnv* env = AttachCurrentThread();
  jni_zero::ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_DirectMatchBridge_directMatchUnitsDownloadFinished(env, obj);
}

void DirectMatchBridge::OnFinishedDownloadingDirectMatchUnitsIcon() {
  JNIEnv* env = AttachCurrentThread();
  jni_zero::ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_DirectMatchBridge_directMatchIconsDownloadFinished(env, obj);
}