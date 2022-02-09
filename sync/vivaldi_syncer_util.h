// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_VIVALDI_SYNCER_UTIL_H_
#define SYNC_VIVALDI_SYNCER_UTIL_H_

#include <string>

namespace syncer {

// Fetch the cache_guid and item_id-based unique notes tag from an update.
// Will return an empty string if someting unexpected happens.
std::string GetUniqueNotesTagFromUpdate(const sync_pb::SyncEntity& update);

}  // namespace syncer

#endif  // SYNC_VIVALDI_SYNCER_UTIL_H_
