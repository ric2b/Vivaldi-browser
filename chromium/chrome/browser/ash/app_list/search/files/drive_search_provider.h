// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_DRIVE_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_DRIVE_SEARCH_PROVIDER_H_

#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/files/file_result.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "components/drive/file_errors.h"

class Profile;

namespace drive {
class DriveIntegrationService;
}  // namespace drive

namespace app_list {

class DriveSearchProvider : public SearchProvider {
 public:
  explicit DriveSearchProvider(Profile* profile,
                               bool should_filter_shared_files = true);
  ~DriveSearchProvider() override;

  DriveSearchProvider(const DriveSearchProvider&) = delete;
  DriveSearchProvider& operator=(const DriveSearchProvider&) = delete;

  void SetQuerySource(
      drivefs::mojom::QueryParameters::QuerySource query_source);

  // SearchProvider:
  ash::AppListSearchResultType ResultType() const override;
  void Start(const std::u16string& query) override;
  void StopQuery() override;

 private:
  void OnSearchDriveByFileName(drive::FileError error,
                               std::vector<drivefs::mojom::QueryItemPtr> items);
  std::unique_ptr<FileResult> MakeResult(const base::FilePath& path,
                                         double relevance,
                                         FileResult::Type type,
                                         const GURL& url);

  bool should_filter_shared_files_;

  // When the query began.
  base::TimeTicks query_start_time_;
  // When DriveFS returned results for the query.
  base::TimeTicks results_returned_time_;

  std::u16string last_query_;
  std::optional<ash::string_matching::TokenizedString> last_tokenized_query_;

  const raw_ptr<Profile> profile_;
  const raw_ptr<drive::DriveIntegrationService> drive_service_;
  drivefs::mojom::QueryParameters::QuerySource query_source_ =
      drivefs::mojom::QueryParameters::QuerySource::kLocalOnly;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<DriveSearchProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_DRIVE_SEARCH_PROVIDER_H_
