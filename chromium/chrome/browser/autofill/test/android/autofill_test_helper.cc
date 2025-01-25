// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/autofill/test/jni_headers/AutofillTestHelper_jni.h"

namespace autofill {

void JNI_AutofillTestHelper_DisableThresholdForCurrentlyShownAutofillPopup(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  if (base::WeakPtr<AutofillSuggestionController> controller =
          ChromeAutofillClient::FromWebContentsForTesting(web_contents)
              ->suggestion_controller_for_testing()) {
    controller->DisableThresholdForTesting(/*disable_threshold=*/true);
  }
}

}  // namespace autofill
