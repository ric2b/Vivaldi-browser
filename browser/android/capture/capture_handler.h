// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#ifndef VIVALDI_BROWSER_ANDROID_CAPTURE_CAPTURE_HANDLER_H_
#define VIVALDI_BROWSER_ANDROID_CAPTURE_CAPTURE_HANDLER_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "browser/thumbnails/capture_page.h"
#include "content/public/browser/web_contents_observer.h"

class Profile;

class CaptureHandler : public content::WebContentsObserver {
 public:
  CaptureHandler(JNIEnv* env, const base::android::JavaRef<jobject>& obj);

  ~CaptureHandler() override;

  void RequestCapturePage(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          const JavaParamRef<jobject>& j_web_contents,
                          const jboolean j_full_page);

 private:
  void OnCapturePageCompleted(vivaldi::CapturePage::Result captured);

  static void ConvertImage(JavaObjectWeakGlobalRef weak_java_ref,
                           vivaldi::CapturePage::Result captured);

  static const int kMaxCaptureHeight = 30000;

  JavaObjectWeakGlobalRef weak_java_ref_;

  base::WeakPtrFactory<CaptureHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CaptureHandler);
};

#endif  // VIVALDI_BROWSER_ANDROID_CAPTURE_CAPTURE_HANDLER_H_
