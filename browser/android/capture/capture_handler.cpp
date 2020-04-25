// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#include "capture_handler.h"

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/task/post_task.h"
#include "chrome/android/chrome_jni_headers/CaptureHandler_jni.h"
#include "chrome/browser/profiles/profile_android.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/android/java_bitmap.h"

#if defined(__ANDROID__)
#include <android/log.h>
#endif

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

CaptureHandler::CaptureHandler(JNIEnv* env,
                               const JavaRef<jobject>& obj)
    : content::WebContentsObserver(),
      weak_java_ref_(env, obj),
      weak_factory_(this) {
}

static jlong JNI_CaptureHandler_Init(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj) {
  CaptureHandler* delegate = new CaptureHandler(env, obj);
  return reinterpret_cast<intptr_t>(delegate);
}

CaptureHandler::~CaptureHandler() {}

void CaptureHandler::RequestCapturePage(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_web_contents,
    const jboolean j_full_page) {
  vivaldi::CapturePage::CaptureParams capture_params;
  capture_params.full_page = j_full_page;
  capture_params.rect = gfx::Rect(0, 0, 0, kMaxCaptureHeight);

  vivaldi::CapturePage::Capture(
      content::WebContents::FromJavaWebContents(j_web_contents), capture_params,
      base::BindOnce(&CaptureHandler::OnCapturePageCompleted,
                     weak_factory_.GetWeakPtr()));
}

void CaptureHandler::OnCapturePageCompleted(
    vivaldi::CapturePage::Result captured) {
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&CaptureHandler::ConvertImage, weak_java_ref_,
                     std::move(captured)));
}

void CaptureHandler::ConvertImage(JavaObjectWeakGlobalRef weak_java_ref,
                                  vivaldi::CapturePage::Result captured) {
  SkBitmap bitmap;
  captured.MovePixelsToBitmap(&bitmap);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref.get(env);
  if (obj.is_null())
    return;

  Java_CaptureHandler_onCapturePageRetrieved(
      env, obj,
      bitmap.drawsNothing() ? NULL : gfx::ConvertToJavaBitmap(&bitmap));
}
