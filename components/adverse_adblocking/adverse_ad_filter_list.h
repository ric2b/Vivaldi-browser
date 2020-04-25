// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_ADVERSE_ADBLOCKING_ADVERSE_AD_FILTER_LIST_H_
#define COMPONENTS_ADVERSE_ADBLOCKING_ADVERSE_AD_FILTER_LIST_H_

#include <set>
#include <string>

#include "base/callback.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

class GURL;

class AdverseAdFilterListService : public KeyedService,
                                   public content::NotificationObserver {
  friend struct base::DefaultSingletonTraits<AdverseAdFilterListService>;

 public:
  typedef base::OnceCallback<void()> ListFailedCallback;

  AdverseAdFilterListService(Profile* profile);

  ~AdverseAdFilterListService() override;

  void LoadList(ListFailedCallback callback);
  void LoadList(const base::FilePath& json_filename,
                ListFailedCallback callback);
  void LoadListOnIO(const base::FilePath& json_filename,
                    ListFailedCallback callback);

  // This will load and parse the block list in addition to compute a MD5
  // checksum for the file-contents.
  void LoadAndInitializeFromString(ListFailedCallback callback,
                                   const std::string* json_string);

  bool IsSiteInList(const GURL&);
  bool has_sites() const { return !adverse_ad_sites_.empty(); }

  void AddBlockItem(const std::string& new_site);
  void ClearSiteList();

  // content::NotificationObserver override.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  static base::FilePath GetDefaultFilePath();

  static std::string* ReadFileToString(const base::FilePath& json_filename,
                                       ListFailedCallback callback);

 private:
  static void PostCallback(ListFailedCallback callback);

  void SettingsUpdated();
  void OnFileExistsChecked(bool exists);
  void OnBlocklistDownloadDone(base::FilePath response_path);
  void OnDoBlockListLifecycleCheck();
  // Will create a |SimpleURLLoader| and download to file on the IO thread.
  // When the file is done we will load the file contents and initialize
  // this class.
  void DownloadBlockList();
  void OnProfileAndServicesInitialized();
  // Will download a checksum and compare this with the local file.
  void DoChecksumBeforeDownload();
  void OnSHA256SumDownloadDone(std::unique_ptr<std::string> response_body);

  // Computes a md5 checksum of an exsisting file,download and compares
  // this to what the checksum is for the file on server. If there is no
  // checksum-file on the server we will always download a new file.
  void ComputeSHA256Sum(const void* data, size_t length);

  std::set<std::string> adverse_ad_sites_;
  Profile* profile_;
  bool blocklist_file_exists_ = false;
  bool is_enabled_and_been_loaded_ = false;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  PrefChangeRegistrar pref_change_registrar_;

  std::string sha256_sum_;

  content::NotificationRegistrar notification_registrar_;

  // The task runner where file I/O is done.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<AdverseAdFilterListService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AdverseAdFilterListService);
};

#endif  // COMPONENTS_ADVERSE_ADBLOCKING_ADVERSE_AD_FILTER_LIST_H_
