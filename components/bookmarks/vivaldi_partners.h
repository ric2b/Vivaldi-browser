// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_BOOKMARKS_VIVALDI_PARTNERS_H_
#define COMPONENTS_BOOKMARKS_VIVALDI_PARTNERS_H_

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/values.h"

namespace vivaldi_partners {

bool HasPartnerThumbnailPrefix(base::StringPiece url);

// Helper to read a JSON asset related to a bookmark partners.
class AssetReader {
 public:
  AssetReader();
  ~AssetReader();

  // Log not found paths as errors.
  void set_log_not_found() {
    log_not_found_ = true;
  }

  // Read bookmark-related json asset. Return nullopt on errors or when the file
  // was not found. Use is_not_found() to distinguish these cases.
  base::Optional<base::Value> ReadJson(base::StringPiece name);

  // Get full path of the file that ReadJson() tried to read.
  std::string GetPath() const;

  bool is_not_found() const {
    return not_found_;
  }

 private:
#if defined(OS_ANDROID)
  std::string path_;
#else
  base::FilePath asset_dir_;
  base::FilePath path_;
#endif  // OS_ANDROID
  bool not_found_ = false;
  bool log_not_found_ = false;
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
  std::string guid;
  std::string guid2;
  std::string thumbnail;
  bool folder = false;
  bool speeddial = false;

  DISALLOW_COPY_AND_ASSIGN(PartnerDetails);
};

class PartnerDatabase {
public:
  PartnerDatabase();
  ~PartnerDatabase();

  static std::unique_ptr<PartnerDatabase> Read();

  const PartnerDetails* FindDetailsByName(base::StringPiece name) const;

  // If id is an old locale-based partner id, change it to the coresponding
  // locale-independent GUID and return true. Otherwise return false and leave
  // the id unchanged.
  bool MapLocaleIdToGUID(std::string& id) const;

private:
 bool ParseJson(base::Value root, base::Value partners_locale_value);

 std::vector<PartnerDetails> details_list_;

 // Map old locale-based partner id to the guid or guid2 if the old id is for
 // an url under Bookmarks folder.
 base::flat_map<std::string, base::StringPiece> locale_id_guid_map_;

 // Map partner details name to its index in the details array.
 base::flat_map<base::StringPiece, const PartnerDetails*> name_index_;

 // The maps above contains references, so prevent copy/assignment for safety.
 DISALLOW_COPY_AND_ASSIGN(PartnerDatabase);
};

}  // namespace vivaldi_partners

#endif  // COMPONENTS_BOOKMARKS_VIVALDI_PARTNERS_H_
