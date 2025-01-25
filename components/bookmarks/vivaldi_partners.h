// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_BOOKMARKS_VIVALDI_PARTNERS_H_
#define COMPONENTS_BOOKMARKS_VIVALDI_PARTNERS_H_

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/uuid.h"
#include "base/values.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace vivaldi_partners {

const std::string GetBookmarkResourceDir();

struct PartnerDetails {
  PartnerDetails();
  ~PartnerDetails();
  PartnerDetails(PartnerDetails&&);
  PartnerDetails& operator=(PartnerDetails&&);

  PartnerDetails(const PartnerDetails&) = delete;
  PartnerDetails& operator=(const PartnerDetails&) = delete;

  // uuid2 is a UUID for a bookmark defined in the Bookmark folder as opposite
  // to SpeedDial. For some partners we define them twice both in SpeedDial and
  // Bookmarks so if the user deletes the speeddial the version in Bookmarks
  // will be used instead.
  std::string name;
  std::string title;
  base::Uuid uuid;
  base::Uuid uuid2;
  std::string thumbnail;
  std::string favicon;
  std::string favicon_url;
  bool folder = false;
  bool speeddial = false;
};

const PartnerDetails* FindDetailsByName(std::string_view name);

// If id is an old locale-based partner id, change it to the coresponding
// locale-independent UUID and return true. Otherwise return false and leave
// the id unchanged.
bool MapLocaleIdToUuid(base::Uuid& id);

// Return an empty string if partner_id is not known or does not have a
// thumbnail.
const std::string& GetThumbnailUrl(const base::Uuid& partner_id);

// Load the partner database from a worker thread and store the result on the
// main thread using passed reference.
void LoadOnWorkerThread(
    const scoped_refptr<base::SequencedTaskRunner>& main_thread_task_runner);

}  // namespace vivaldi_partners

#endif  // COMPONENTS_BOOKMARKS_VIVALDI_PARTNERS_H_
