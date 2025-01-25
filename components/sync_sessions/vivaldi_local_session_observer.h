// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_SYNC_SESSIONS_VIV_SPECIFIC_OBSERVER_H_
#define COMPONENTS_SYNC_SESSIONS_VIV_SPECIFIC_OBSERVER_H_

#include "chromium/components/prefs/pref_change_registrar.h"
#include "chromium/components/sync_device_info/device_info_tracker.h"

class Profile;

namespace syncer {
class DeviceInfoSyncService;
}

namespace vivaldi {

class VivaldiLocalSessionObserver : public syncer::DeviceInfoTracker::Observer {
 public:
  explicit VivaldiLocalSessionObserver(Profile* profile);
  virtual ~VivaldiLocalSessionObserver();

  void TriggerSync();

 private:
  void OnSpecificPrefsChanged(const std::string& path);
  void OnSessionNamePrefsChanged(const std::string& path);

  void OnDeviceInfoChange() override;
  void OnDeviceInfoShutdown() override;

  void UpdateSession();
  void DeregisterDeviceInfoObservers();

  PrefChangeRegistrar session_name_prefs_registrar_;
  PrefChangeRegistrar specific_prefs_registrar_;

  const raw_ptr<Profile> profile_;
  raw_ptr<syncer::DeviceInfoSyncService> device_info_service_;
};

} // namespace vivaldi

#endif // COMPONENTS_SYNC_SESSIONS_VIV_SPECIFIC_OBSERVER_H_
