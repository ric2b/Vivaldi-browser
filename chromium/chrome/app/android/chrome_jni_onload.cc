// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/android/chrome_jni_onload.h"

#include "chrome/app/android/chrome_main_delegate_android.h"
#include "content/public/app/content_jni_onload.h"
#include "content/public/app/content_main.h"

// Vivaldi
#include "extraparts/vivaldi_main_delegate.h"

namespace android {

bool OnJNIOnLoadInit() {
  if (!content::android::OnJNIOnLoadInit())
    return false;

#if defined(VIVALDI_BUILD)
  content::SetContentMainDelegate(new VivaldiMainDelegate());
#else
  content::SetContentMainDelegate(new ChromeMainDelegateAndroid());
#endif
  return true;
}

}  // namespace android
