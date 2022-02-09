// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_BOOKMARKS_VIVALDI_PARTNERS_H_
#define COMPONENTS_BOOKMARKS_VIVALDI_PARTNERS_H_

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace vivaldi_partners {

extern const char kBookmarkResourceDir[];

struct PartnerDetails {
  PartnerDetails();
  ~PartnerDetails();
  PartnerDetails(PartnerDetails&&);
  PartnerDetails& operator=(PartnerDetails&&);

  PartnerDetails(const PartnerDetails&) = delete;
  PartnerDetails& operator=(const PartnerDetails&) = delete;

  // guid2 is a GUID for a bookmark defined in the Bookmark folder as opposite
  // to SpeedDial. For some partners we define them twice both in SpeedDial and
  // Bookmarks so if the user deletes the speeddial the version in Bookmarks
  // will be used instead.
  std::string name;
  std::string title;
  base::GUID guid;
  base::GUID guid2;
  std::string thumbnail;
  bool folder = false;
  bool speeddial = false;
};

const PartnerDetails* FindDetailsByName(base::StringPiece name);

// If id is an old locale-based partner id, change it to the coresponding
// locale-independent GUID and return true. Otherwise return false and leave
// the id unchanged.
bool MapLocaleIdToGUID(base::GUID& id);

// Return an empty string if partner_id is not known or does not have a
// thumbnail.
const std::string& GetThumbnailUrl(const base::GUID& partner_id);

// Load the partner database from a worker thread and store the result on the
// main thread using passed reference.
void LoadOnWorkerThread(
    const scoped_refptr<base::SequencedTaskRunner>& main_thread_task_runner);

}  // namespace vivaldi_partners

#endif  // COMPONENTS_BOOKMARKS_VIVALDI_PARTNERS_H_
