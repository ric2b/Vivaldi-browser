// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace autofill {

// Exposes some testing operations for ContentAutofillDriver.
class ContentAutofillDriverTestApi {
 public:
  explicit ContentAutofillDriverTestApi(ContentAutofillDriver* driver)
      : driver_(*driver) {}

  void set_autofill_manager(std::unique_ptr<AutofillManager> autofill_manager) {
    driver_->autofill_manager_ = std::move(autofill_manager);
  }

  void LiftForTest(FormData& form) const { driver_->LiftForTest(form); }

 private:
  const raw_ref<ContentAutofillDriver> driver_;
};

inline ContentAutofillDriverTestApi test_api(ContentAutofillDriver& driver) {
  return ContentAutofillDriverTestApi(&driver);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_TEST_API_H_
