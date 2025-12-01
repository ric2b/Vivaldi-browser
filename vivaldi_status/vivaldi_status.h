// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef VIVALDI_STATUS_H_
#define VIVALDI_STATUS_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "base/timer/timer.h"

#if !BUILDFLAG(IS_IOS)
class Profile;
#endif

namespace vivaldi_status {

class VivaldiStatus : public KeyedService {
 public:
  // These values must match ids in json
  // Used in iterations. Update code if no longer possible,
  enum Services {
    kServiceMin = 1,
    kVivaldiCom = kServiceMin,
    kAutoUpdate = 2,
    kSync = 3,
    kVivaldiNet = 4,
    kLogin = 5,
    kForum = 6,
    kIncomingMail = 7,
    kOutgoingMail = 8,
    kImapPOP = 9,
    kWebmail = 10,
    kBlogs = 11,
    kCalDAV = 12,
    kTranslationService = 13,  // Not in use.
    kThemes = 14,
    kMastodon = 15,
    kServiceMax = kMastodon,
  };

  // These values must match ids in status entry in json
  enum Mode {
    kUnknown     = 0,
    kOperational = 1,
    kMaintenance = 2,
    kMinorOutage = 3,
    kMajorOutage = 4,
  };
  struct Health {
    bool operator==(const Health &rhs) const
        { return id == rhs.id && mode == rhs.mode; }
    Services id;
    Mode mode;
  };
  typedef std::map<std::string, Health> IdToHealthMap;
  typedef std::map<std::string, bool> IdToBoolMap;

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() = default;
    virtual void OnVivaldiStatusUpdated(VivaldiStatus* status,
                                        std::vector<Health> changes) {}
    virtual void OnVivaldiSyncStatusUpdated(Mode mode) {}
    virtual void OnVivaldiStatusError(VivaldiStatus* status) {}
  };

  VivaldiStatus();
  ~VivaldiStatus() = default;

#if BUILDFLAG(IS_IOS)
  void Init(
      const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory);
#else
  // report_all_changes should be set to true when all updates, regardless if a
  // value has changed or not, should trigger a call to the observers. Typically
  // only needed for platforms that can have multiple windows.
  void Init(content::BrowserContext* context, bool report_all_changes);
#endif

  // Returns true if saved data is recent enough to be used.
  bool IsValid();

  // See GetMode
  bool GetSyncMode(Mode* mode) { return GetMode(kSync, mode); }

  // Returns true if a download was started or requested, false if saved data is
  // already valid. Note on usage: Refreshing state will fetch data from our
  // servers. We should only do this if we have detected a problem in the client
  // for supported services (Sync at the moment) that can not be resolved there
  // and only after GetMode() returns false.
  bool Refresh(Services service);

  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

 private:
  void Download();
  void OnDownloadDone(const size_t loader_idx,
                      std::unique_ptr<std::string> response_body);
  bool Parse(std::unique_ptr<std::string> response_body);

  // Looks up state of a particular service. Returned 'mode' is only valid if
  // function returns true.
  bool GetMode(Services service, Mode* mode);

  std::string ServiceToId(Services service);
  bool IdToService(std::string id, Services* service);

  base::Time last_attempted_update_;
  base::Time last_sucessful_update_;
  bool is_updating_ = false;
  bool report_all_changes_ = false;

  base::ObserverList<Observer> observers_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::vector<std::unique_ptr<network::SimpleURLLoader>> simple_url_loader_;
  base::OneShotTimer timer_;
  IdToHealthMap id_to_health_map_;
  IdToBoolMap request_map_;

  base::WeakPtrFactory<VivaldiStatus> weak_factory_{this};
};

}  // vivaldi_status

#endif  // VIVALDI_STATUS_H_