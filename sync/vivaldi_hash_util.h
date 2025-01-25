// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_VIVALDI_HASH_UTIL_H_
#define SYNC_VIVALDI_HASH_UTIL_H_

#include <string>

#include "components/sync/base/unique_position.h"

namespace syncer {

// A helper for generating the notes type's tag.  This is required in more
// than one place, so we define the algorithm here to make sure the
// implementation is consistent.
syncer::UniquePosition::Suffix GenerateSyncableNotesHash(
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id);

}  // namespace syncer

#endif  // SYNC_VIVALDI_HASH_UTIL_H_
