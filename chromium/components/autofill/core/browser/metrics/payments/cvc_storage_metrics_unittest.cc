// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/cvc_storage_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/payments_data_manager.h"

namespace autofill::autofill_metrics {

class CvcStorageMetricsTest : public AutofillMetricsBaseTest,
                              public testing::Test {
 public:
  CvcStorageMetricsTest() = default;
  ~CvcStorageMetricsTest() override = default;

  const FormData& form() const { return form_; }
  const CreditCard& card() const { return card_; }

  void SetUp() override {
    SetUpHelper();
    // Set up the form data. Reset form action to skip the IsFormMixedContent
    // check.
    form_ =
        GetAndAddSeenForm({.description_for_logging = "CvcStorage",
                           .fields = {{.role = CREDIT_CARD_NAME_FULL},
                                      {.role = CREDIT_CARD_NUMBER},
                                      {.role = CREDIT_CARD_EXP_MONTH},
                                      {.role = CREDIT_CARD_EXP_2_DIGIT_YEAR},
                                      {.role = CREDIT_CARD_VERIFICATION_CODE}},
                           .action = ""});

    // Add a masked server card.
    card_ = test::WithCvc(test::GetMaskedServerCard());
    personal_data().test_payments_data_manager().AddServerCreditCard(card_);
  }

  void TearDown() override { TearDownHelper(); }

 private:
  CreditCard card_;
  FormData form_;
};

// Test CVC suggestion shown metrics are correctly logged.
TEST_F(CvcStorageMetricsTest, LogShownMetrics) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  personal_data().test_payments_data_manager().SetIsPaymentCvcStorageEnabled(
      true);

  // Simulate activating the autofill popup for the credit card field.
  autofill_manager().OnAskForValuesToFillTest(
      form(), form().fields().front().global_id());
  DidShowAutofillSuggestions(form(), /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          base::Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SHOWN, 1),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SHOWN_ONCE, 1)));

  // Show the popup again.
  autofill_manager().OnAskForValuesToFillTest(
      form(), form().fields().front().global_id());
  DidShowAutofillSuggestions(form(), /*field_index=*/0,
                             SuggestionType::kCreditCardEntry);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          base::Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 2),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SHOWN, 2),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SHOWN_ONCE, 1)));
}

// Test CVC suggestion selected metrics are correctly logged.
TEST_F(CvcStorageMetricsTest, LogSelectedMetrics) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);

  personal_data().test_payments_data_manager().SetIsPaymentCvcStorageEnabled(
      true);

  // Simulate selecting the suggestion with CVC.
  autofill_manager().OnAskForValuesToFillTest(
      form(), form().fields().back().global_id());
  DidShowAutofillSuggestions(form(), /*field_index=*/form().fields().size() - 1,
                             SuggestionType::kCreditCardEntry);
  autofill_manager().AuthenticateThenFillCreditCardForm(
      form(), form().fields().back(),
      *personal_data().payments_data_manager().GetCreditCardByInstrumentId(
          card().instrument_id()),
      {.trigger_source = AutofillTriggerSource::kPopup});

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          base::Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SELECTED, 1),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SELECTED_ONCE,
                       1)));

  // Simulate selecting the suggestion again.
  autofill_manager().AuthenticateThenFillCreditCardForm(
      form(), form().fields().front(),
      *personal_data().payments_data_manager().GetCreditCardByInstrumentId(
          card().instrument_id()),
      {.trigger_source = AutofillTriggerSource::kPopup});

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          base::Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 2),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SELECTED, 2),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SELECTED_ONCE,
                       1)));
}

// Test CVC suggestion filled metrics are correctly logged.
TEST_F(CvcStorageMetricsTest, LogFilledMetrics) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);

  personal_data().test_payments_data_manager().SetIsPaymentCvcStorageEnabled(
      true);

  // Simulate filling the suggestion with CVC.
  autofill_manager().AuthenticateThenFillCreditCardForm(
      form(), form().fields().front(),
      *personal_data().payments_data_manager().GetCreditCardByInstrumentId(
          card().instrument_id()),
      {.trigger_source = AutofillTriggerSource::kPopup});
  test_api(autofill_manager())
      .OnCreditCardFetched(CreditCardFetchResult::kSuccess, &card());

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          base::Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_FILLED, 1),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_FILLED_ONCE,
                       1)));

  // Fill the suggestion again.
  autofill_manager().AuthenticateThenFillCreditCardForm(
      form(), form().fields().front(),
      *personal_data().payments_data_manager().GetCreditCardByInstrumentId(
          card().instrument_id()),
      {.trigger_source = AutofillTriggerSource::kPopup});
  test_api(autofill_manager())
      .OnCreditCardFetched(CreditCardFetchResult::kSuccess, &card());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          base::Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 2),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_FILLED, 2),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_FILLED_ONCE,
                       1)));
}

// Test will submit and submitted metrics are correctly logged.
TEST_F(CvcStorageMetricsTest, LogSubmitMetrics) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);

  personal_data().test_payments_data_manager().SetIsPaymentCvcStorageEnabled(
      true);

  // Simulate filling and then submitting the card with CVC.
  autofill_manager().OnAskForValuesToFillTest(
      form(), form().fields().front().global_id());
  autofill_manager().AuthenticateThenFillCreditCardForm(
      form(), form().fields().front(),
      *personal_data().payments_data_manager().GetCreditCardByInstrumentId(
          card().instrument_id()),
      {.trigger_source = AutofillTriggerSource::kPopup});
  test_api(autofill_manager())
      .OnCreditCardFetched(CreditCardFetchResult::kSuccess, &card());
  SubmitForm(form());

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          base::Bucket(
              FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE, 1),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_WILL_SUBMIT_ONCE,
                       1),
          base::Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                       1),
          base::Bucket(FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SUBMITTED_ONCE,
                       1)));
}

}  // namespace autofill::autofill_metrics
