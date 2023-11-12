// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_ADVERSE_ADBLOCKING_ADVERSE_AD_FILTER_LIST_H_
#define COMPONENTS_ADVERSE_ADBLOCKING_ADVERSE_AD_FILTER_LIST_H_

#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

class GURL;
class Profile;

class AdverseAdFilterListService : public KeyedService,
                                   public ProfileManagerObserver {
  friend struct base::DefaultSingletonTraits<AdverseAdFilterListService>;

 public:

  AdverseAdFilterListService(Profile* profile);
  ~AdverseAdFilterListService() override;
  AdverseAdFilterListService(const AdverseAdFilterListService&) = delete;
  AdverseAdFilterListService& operator=(const AdverseAdFilterListService&) =
      delete;

  void LoadList(const base::FilePath& json_filename);

  // This will load and parse the block list in addition to compute a MD5
  // checksum for the file-contents.
  void LoadAndInitializeFromString(const std::string* json_string);

  bool IsSiteInList(const GURL&);
  bool has_sites() const { return !adverse_ad_sites_.empty(); }

  void AddBlockItem(const std::string& new_site);
  void ClearSiteList();

  // ProfileManagerObserver
  void OnProfileAdded(Profile* profile) override;

  static base::FilePath GetDefaultFilePath();

  static std::string* ReadFileToString(const base::FilePath& json_filename);

 private:

  void SettingsUpdated();
  void OnFileExistsChecked(bool exists);
  void OnBlocklistDownloadDone(base::FilePath response_path);
  void OnDoBlockListLifecycleCheck();
  // Will create a |SimpleURLLoader| and download to file on the IO thread.
  // When the file is done we will load the file contents and initialize
  // this class.
  void DownloadBlockList();
  // Will download a checksum and compare this with the local file.
  void DoChecksumBeforeDownload();
  void OnSHA256SumDownloadDone(std::unique_ptr<std::string> response_body);

  // Computes a md5 checksum of an exsisting file,download and compares
  // this to what the checksum is for the file on server. If there is no
  // checksum-file on the server we will always download a new file.
  void ComputeSHA256Sum(const void* data, size_t length);

  std::set<std::string> adverse_ad_sites_;
  const raw_ptr<Profile> profile_;
  bool blocklist_file_exists_ = false;
  bool is_enabled_and_been_loaded_ = false;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  PrefChangeRegistrar pref_change_registrar_;

  std::string sha256_sum_;

  // The task runner where file I/O is done.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<AdverseAdFilterListService> weak_ptr_factory_;
};

#endif  // COMPONENTS_ADVERSE_ADBLOCKING_ADVERSE_AD_FILTER_LIST_H_
