// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_SYNC_SESSIONS_VIV_SPECIFIC_OBSERVER_H_
#define COMPONENTS_SYNC_SESSIONS_VIV_SPECIFIC_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync_device_info/device_info_tracker.h"

#if BUILDFLAG(IS_IOS)
class ProfileIOS;
using ProfileClass = ProfileIOS;

namespace sync_sessions {
  class SyncSessionsClient;
}
#else
class Profile;
using ProfileClass = Profile;
#endif

namespace syncer {
class DeviceInfoSyncService;
}

namespace vivaldi {

class VivaldiLocalSessionObserver : public syncer::DeviceInfoTracker::Observer {
 public:
#if BUILDFLAG(IS_IOS)
  explicit VivaldiLocalSessionObserver(
      ProfileClass* profile,
      sync_sessions::SyncSessionsClient* sessions_client);
#else
  explicit VivaldiLocalSessionObserver(ProfileClass* profile);
#endif
  virtual ~VivaldiLocalSessionObserver();

  void TriggerSync();

 private:
  void OnSpecificPrefsChanged(const std::string& path);
  void OnSessionNamePrefsChanged(const std::string& path);

  void OnDeviceInfoChange() override;
  void OnDeviceInfoShutdown() override;

  void UpdateSession();
  void SetFallbackDeviceName(const std::string& device_name);
  void DeregisterDeviceInfoObservers();

  PrefChangeRegistrar session_name_prefs_registrar_;
  PrefChangeRegistrar specific_prefs_registrar_;

  const raw_ptr<ProfileClass> profile_;
  raw_ptr<syncer::DeviceInfoSyncService> device_info_service_;
  std::string fallback_device_name_;

#if BUILDFLAG(IS_IOS)
  const raw_ptr<sync_sessions::SyncSessionsClient> sessions_client_;
#endif

  base::WeakPtrFactory<VivaldiLocalSessionObserver> weak_factory_{this};
};

} // namespace vivaldi

#endif // COMPONENTS_SYNC_SESSIONS_VIV_SPECIFIC_OBSERVER_H_
