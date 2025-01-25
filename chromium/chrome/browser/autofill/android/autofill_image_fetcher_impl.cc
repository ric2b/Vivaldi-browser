// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/autofill_image_fetcher_impl.h"

#include "base/android/jni_android.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/autofill/android/jni_headers/AutofillImageFetcher_jni.h"

namespace autofill {

AutofillImageFetcherImpl::AutofillImageFetcherImpl(ProfileKey* key)
    : key_(key) {}

AutofillImageFetcherImpl::~AutofillImageFetcherImpl() = default;

void AutofillImageFetcherImpl::FetchImagesForURLs(
    base::span<const GURL> card_art_urls,
    base::OnceCallback<void(
        const std::vector<std::unique_ptr<CreditCardArtImage>>&)> callback) {
  if (card_art_urls.empty()) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();

  Java_AutofillImageFetcher_prefetchImages(env, GetOrCreateJavaImageFetcher(),
                                           card_art_urls);
}

base::android::ScopedJavaLocalRef<jobject>
AutofillImageFetcherImpl::GetOrCreateJavaImageFetcher() {
  if (!java_image_fetcher_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    java_image_fetcher_ = Java_AutofillImageFetcher_create(
        env, key_->GetProfileKeyAndroid()->GetJavaObject());
  }

  return base::android::ScopedJavaLocalRef<jobject>(java_image_fetcher_);
}

}  // namespace autofill
