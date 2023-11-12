// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULES_INDEX_BUILDER_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULES_INDEX_BUILDER_H_

#include "components/request_filter/adblock_filter/adblock_rules_index_manager.h"

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback.h"

namespace adblock_filter {

using IndexSavedCallback = base::OnceCallback<void(const std::string&)>;

// Constructs a new flat buffer storing the indexing informations for the rules
// and saves it. None of the parameters passed, except for
// |index_saved_callback| are used beyond the lifetime of this function.
void BuildAndSaveIndex(
    const std::map<uint32_t, std::unique_ptr<RuleBufferHolder>>& rules_buffers,
    base::SequencedTaskRunner* file_task_runner,
    const base::FilePath& index_path,
    IndexSavedCallback index_saved_callback);

}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULES_INDEX_BUILDER_H_
