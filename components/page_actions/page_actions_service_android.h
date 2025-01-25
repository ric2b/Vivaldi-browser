// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_PAGE_ACTIONS_PAGE_ACTIONS_ANDROID_H_
#define COMPONENTS_PAGE_ACTIONS_PAGE_ACTIONS_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "components/page_actions/page_actions_service.h"

class Profile;

class PageActionsServiceAndroid : public page_actions::Service::Observer {
 public:
  PageActionsServiceAndroid(JNIEnv* env, const base::android::JavaRef<jobject>& obj);
  PageActionsServiceAndroid(const PageActionsServiceAndroid& other) = delete;
  PageActionsServiceAndroid& operator=(const PageActionsServiceAndroid& other) =
      delete;
  ~PageActionsServiceAndroid() override;

  base::android::ScopedJavaLocalRef<jobjectArray> GetScripts(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean SetScriptOverrideForTab(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& tab_contents,
      const base::android::JavaParamRef<jstring>& script,
      jint script_override);
  base::android::ScopedJavaLocalRef<jobjectArray> GetScriptOverridesForTab(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& tab_contents,
      jint script_override);

  // page_actions::Service::Observer implementation
  void OnServiceLoaded(page_actions::Service* service) override;
  void OnScriptPathsChanged() override;

 private:
  raw_ptr<page_actions::Service> service_;

  JavaObjectWeakGlobalRef weak_java_ref_;
};

#endif  // COMPONENTS_PAGE_ACTIONS_PAGE_ACTIONS_ANDROID_H_
