// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_constants.h"

#include "app/vivaldi_constants.h"

namespace optimization_guide {

const base::FilePath::CharType kUnindexedHintsFileName[] =
    FILE_PATH_LITERAL("optimization-hints.pb");

const char kRulesetFormatVersionString[] = "1.0.0";

const char kOptimizationGuideServiceGetHintsDefaultURL[] =
    KNOWN_404("/v1:GetHints");

const char kOptimizationGuideServiceGetModelsDefaultURL[] =
    KNOWN_404("/v1:GetModels");

const char kLoadedHintLocalHistogramString[] =
    "OptimizationGuide.LoadedHint.Result";

const base::FilePath::CharType kOptimizationGuideHintStore[] =
    FILE_PATH_LITERAL("optimization_guide_hint_cache_store");

const base::FilePath::CharType
    kOptimizationGuidePredictionModelMetadataStore[] =
        FILE_PATH_LITERAL("optimization_guide_model_metadata_store");

const base::FilePath::CharType kOptimizationGuidePredictionModelDownloads[] =
    FILE_PATH_LITERAL("optimization_guide_prediction_model_downloads");

const base::FilePath::CharType kPageEntitiesMetadataStore[] =
    FILE_PATH_LITERAL("page_content_annotations_page_entities_metadata_store");

}  // namespace optimization_guide
