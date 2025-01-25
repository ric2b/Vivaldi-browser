// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_RESPONSE_HOLDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_RESPONSE_HOLDER_H_

#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"

namespace optimization_guide {

class ResponseHolder {
 public:
  ResponseHolder();
  ~ResponseHolder();

  OptimizationGuideModelExecutionResultStreamingCallback callback();

  const std::optional<std::string>& value() const { return response_received_; }
  const std::vector<std::string>& streamed() const {
    return streamed_responses_;
  }
  const std::optional<
      OptimizationGuideModelExecutionError::ModelExecutionError>&
  error() const {
    return response_error_;
  }
  const std::optional<bool>& provided_by_on_device() const {
    return provided_by_on_device_;
  }
  ModelQualityLogEntry* log_entry() { return log_entry_received_.get(); }

 private:
  void OnResponse(OptimizationGuideModelStreamingExecutionResult result);

  std::vector<std::string> streamed_responses_;
  std::optional<std::string> response_received_;
  std::optional<bool> provided_by_on_device_;
  std::unique_ptr<ModelQualityLogEntry> log_entry_received_;
  std::optional<OptimizationGuideModelExecutionError::ModelExecutionError>
      response_error_;
  base::WeakPtrFactory<ResponseHolder> weak_ptr_factory_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_RESPONSE_HOLDER_H_
