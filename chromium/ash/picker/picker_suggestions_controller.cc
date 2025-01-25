// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_suggestions_controller.h"

#include "ash/picker/model/picker_mode_type.h"
#include "ash/picker/model/picker_model.h"
#include "ash/picker/picker_clipboard_provider.h"
#include "ash/picker/search/picker_date_search.h"
#include "ash/picker/search/picker_math_search.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_client.h"
#include "ash/public/cpp/picker/picker_search_result.h"

namespace ash {
namespace {

constexpr int kMaxRecentFiles = 10;

}

PickerSuggestionsController::PickerSuggestionsController(PickerClient* client)
    : client_(client) {}

PickerSuggestionsController::~PickerSuggestionsController() = default;

std::vector<PickerSearchResult> GetMostRecentResult(
    std::vector<PickerSearchResult> results) {
  if (results.empty()) {
    return {};
  }
  return {results[0]};
}

void PickerSuggestionsController::GetSuggestions(const PickerModel& model,
                                                 SuggestionsCallback callback) {
  if (model.GetMode() == PickerModeType::kUnfocused) {
    std::vector<PickerSearchResult> new_window_results;
    for (PickerSearchResult::NewWindowData::Type type : {
             PickerSearchResult::NewWindowData::Type::kDoc,
             PickerSearchResult::NewWindowData::Type::kSheet,
             PickerSearchResult::NewWindowData::Type::kSlide,
             PickerSearchResult::NewWindowData::Type::kChrome,
         }) {
      new_window_results.push_back(PickerSearchResult::NewWindow(type));
    }
    callback.Run(std::move(new_window_results));
  }

  if (model.GetMode() == PickerModeType::kUnfocused ||
      model.GetMode() == PickerModeType::kNoSelection) {
    callback.Run({PickerSearchResult::CapsLock(!model.is_caps_lock_enabled())});
  }

  if (base::Contains(model.GetAvailableCategories(),
                     PickerCategory::kEditorRewrite)) {
    client_->GetSuggestedEditorResults(callback);
  }

  if (model.GetMode() == PickerModeType::kHasSelection) {
    std::vector<PickerSearchResult> case_transform_results;
    for (PickerSearchResult::CaseTransformData::Type type : {
             PickerSearchResult::CaseTransformData::Type::kUpperCase,
             PickerSearchResult::CaseTransformData::Type::kLowerCase,
             PickerSearchResult::CaseTransformData::Type::kTitleCase,
             PickerSearchResult::CaseTransformData::Type::kSentenceCase,
         }) {
      case_transform_results.push_back(PickerSearchResult::CaseTransform(type));
    }
    callback.Run(std::move(case_transform_results));
  }

  // TODO: b/344685737 - Rank and collect suggestions in a more intelligent way.
  for (PickerCategory category : model.GetRecentResultsCategories()) {
    GetSuggestionsForCategory(
        model, category,
        base::BindRepeating(&GetMostRecentResult).Then(callback));
  }
}

void PickerSuggestionsController::GetSuggestionsForCategory(
    const PickerModel& model,
    PickerCategory category,
    SuggestionsCallback callback) {
  std::vector<ash::PickerSearchResult> recent_results;
  switch (category) {
    case PickerCategory::kEditorWrite:
    case PickerCategory::kEditorRewrite:
      NOTREACHED_NORETURN();
    case PickerCategory::kLinks:
      client_->GetSuggestedLinkResults(std::move(callback));
      return;
    case PickerCategory::kExpressions:
      NOTREACHED_NORETURN();
    case PickerCategory::kDriveFiles:
      client_->GetRecentDriveFileResults(kMaxRecentFiles, std::move(callback));
      return;
    case PickerCategory::kLocalFiles:
      client_->GetRecentLocalFileResults(kMaxRecentFiles, std::move(callback));
      return;
    case PickerCategory::kDatesTimes:
      std::move(callback).Run(PickerSuggestedDateResults());
      break;
    case PickerCategory::kUnitsMaths:
      std::move(callback).Run(PickerMathExamples());
      break;
    case PickerCategory::kClipboard:
      clipboard_provider_.FetchResults(std::move(callback));
      return;
  }
}

}  // namespace ash
