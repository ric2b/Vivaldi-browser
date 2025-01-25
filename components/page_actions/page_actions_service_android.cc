// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/page_actions/page_actions_service_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/PageActionsService_jni.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/page_actions/page_actions_service_factory.h"
#include "components/page_actions/page_actions_tab_helper.h"
#include "content/public/browser/web_contents.h"

static jlong JNI_PageActionsService_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  PageActionsServiceAndroid* page_actions_service =
      new PageActionsServiceAndroid(env, obj);
  return reinterpret_cast<intptr_t>(page_actions_service);
}

PageActionsServiceAndroid::PageActionsServiceAndroid(JNIEnv* env,
  const base::android::JavaRef<jobject>& obj)
    : weak_java_ref_(env, obj) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);
  service_ = page_actions::ServiceFactory::GetForBrowserContext(profile);
  service_->AddObserver(this);
}

PageActionsServiceAndroid::~PageActionsServiceAndroid() {
  service_->RemoveObserver(this);
}

base::android::ScopedJavaLocalRef<jobjectArray>
PageActionsServiceAndroid::GetScripts(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  std::vector<base::FilePath> paths = service_->GetAllScriptPaths();

  std::vector<std::string> result;
  std::transform(paths.begin(), paths.end(), std::back_inserter(result),
                 [](base::FilePath p) { return p.AsUTF8Unsafe(); });

  return base::android::ToJavaArrayOfStrings(env, result);
}

jboolean PageActionsServiceAndroid::SetScriptOverrideForTab(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& tab_contents,
    const base::android::JavaParamRef<jstring>& script,
    jint script_override) {
  return service_->SetScriptOverrideForTab(
      content::WebContents::FromJavaWebContents(tab_contents),
      base::FilePath::FromUTF8Unsafe(
          base::android::ConvertJavaStringToUTF8(env, script)),
          page_actions::Service::ScriptOverride(script_override));
}

base::android::ScopedJavaLocalRef<jobjectArray>
PageActionsServiceAndroid::GetScriptOverridesForTab(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& tab_contents,
    jint script_override) {
  std::vector<std::string> result;
  auto* page_actions_helper = page_actions::TabHelper::FromWebContents(
      content::WebContents::FromJavaWebContents(tab_contents));
  if (page_actions_helper) {
    for (const auto& recorded_script_override :
         page_actions_helper->GetScriptOverrides()) {
      if ((recorded_script_override.second &&
           script_override == page_actions::Service::kEnabledOverride) ||
          (!recorded_script_override.second &&
           script_override == page_actions::Service::kDisabledOverride))
        result.push_back(recorded_script_override.first.AsUTF8Unsafe());
    }
  }
  return base::android::ToJavaArrayOfStrings(env, result);
}

void PageActionsServiceAndroid::OnScriptPathsChanged() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_PageActionsService_onScriptPathsChanged(env, obj);
}

void PageActionsServiceAndroid::OnServiceLoaded(
    page_actions::Service* service) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_PageActionsService_onNativeServiceLoaded(env, obj);
}
