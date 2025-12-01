// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved
#include "ui/web_ui_native_call_utils.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/WebUINativeCallUtils_jni.h"

using base::android::ScopedJavaLocalRef;

namespace webUINativeCalls {
void openNewTab(std::string url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_WebUINativeCallUtils_openNewTab(env, url);
}
void closeActivity(content::WebContents* webContents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_WebUINativeCallUtils_closeActivity(env, webContents);
}
void createPrivacyReportNotification(int64_t adsBlocked, int64_t trackersBlocked) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_WebUINativeCallUtils_createPrivacyReportNotification(env, adsBlocked,
                                                            trackersBlocked);
}
}  // namespace webUINativeCalls
