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
}  // namspace base

namespace vivaldi_partners {

bool HasPartnerThumbnailPrefix(base::StringPiece url);

// Helper to read a JSON asset related to a bookmark partners.
class AssetReader {
 public:
  AssetReader();
  ~AssetReader();

  // Read bookmark-related json asset. Return nullopt on errors or when the file
  // was not found. Use is_not_found() to distinguish these cases.
  base::Optional<base::Value> ReadJson(base::StringPiece name);

  // Get full path of the file that ReadJson() tried to read.
  std::string GetPath() const;

 private:
#if defined(OS_ANDROID)
  std::string path_;
#else
  base::FilePath asset_dir_;
  base::FilePath path_;
#endif  // OS_ANDROID
};

struct PartnerDetails {
  PartnerDetails();
  ~PartnerDetails();
  PartnerDetails(PartnerDetails&&);
  PartnerDetails& operator=(PartnerDetails&&);

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

  DISALLOW_COPY_AND_ASSIGN(PartnerDetails);
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
