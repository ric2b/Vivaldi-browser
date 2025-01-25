// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"

#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_quality/model_quality_util.h"

namespace optimization_guide {

ModelQualityLogEntry::ModelQualityLogEntry(
    std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request,
    base::WeakPtr<ModelQualityLogsUploaderService> uploader)
    : log_ai_data_request_(std::move(log_ai_data_request)),
      uploader_(uploader) {}

ModelQualityLogEntry::~ModelQualityLogEntry() {
  // Upload logs upon destruction. Typical usage will destroy a log entry
  // intentionally in order to trigger upload. However, uploading upon
  // destruction also covers the case when the logs are not explicitly uploaded
  // in the feature code -- for example, when Chrome is closed.

  // Bail early if there's nothing to upload. The uploader will not exist if
  // uploading is not allowed -- for example, in Incognito mode.
  if (!log_ai_data_request_) {
    return;
  }
  auto feature = GetModelExecutionFeature(log_ai_data_request_->feature_case());
  if (!feature || !uploader_ || !uploader_->CanUploadLogs(*feature)) {
    return;
  }

  uploader_->UploadModelQualityLogs(std::move(log_ai_data_request_));
}

// static
void ModelQualityLogEntry::Upload(std::unique_ptr<ModelQualityLogEntry> entry) {
  // Destroying the log entry triggers an upload.
  entry.reset();
}

// static
void ModelQualityLogEntry::Drop(std::unique_ptr<ModelQualityLogEntry> entry) {
  // Clearing the data results in dropping the log.
  entry->log_ai_data_request_.reset();
}

}  // namespace optimization_guide
