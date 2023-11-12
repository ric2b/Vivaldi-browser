// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_VIVALDI_SYNC_UI_HELPER_H_
#define SYNC_VIVALDI_SYNC_UI_HELPER_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/protocol/sync_protocol_error.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

class Profile;

namespace vivaldi {

class VivaldiSyncServiceImpl;
class VivaldiAccountManager;

enum EngineState {
  STOPPED = 0,
  STARTING,
  STARTING_SERVER_ERROR,
  STARTED,
  CLEARING_DATA,
  CONFIGURATION_PENDING,
  FAILED
};

struct EngineData {
  EngineData();
  EngineData(const EngineData& other);
  ~EngineData();

  EngineState engine_state;
  syncer::SyncService::DisableReasonSet disable_reasons;
  syncer::SyncProtocolErrorType protocol_error_type;
  std::string protocol_error_description;
  syncer::ClientAction protocol_error_client_action;
  bool uses_encryption_password;
  bool needs_decryption_password;
  bool is_encrypting_everything;
  bool is_setup_in_progress;
  bool is_first_setup_complete;
  bool sync_everything;
  syncer::UserSelectableTypeSet data_types;
};

class VivaldiSyncUIHelper : public syncer::SyncServiceObserver {
 public:
  using ResultCallback = base::OnceCallback<void(bool)>;

  enum CycleStatus {
    NOT_SYNCED = 0,
    SUCCESS,
    AUTH_ERROR,
    SERVER_ERROR,
    NETWORK_ERROR,
    CONFLICT,
    THROTTLED,
    OTHER_ERROR
  };

  struct CycleData {
    CycleStatus download_updates_status;
    CycleStatus commit_status;
    base::Time cycle_start_time;
    base::Time next_retry_time;
  };

  VivaldiSyncUIHelper(VivaldiSyncServiceImpl* sync_service,
                      VivaldiAccountManager* account_manager);
  ~VivaldiSyncUIHelper() override;

  void RegisterObserver();

  bool SetEncryptionPassword(const std::string& password);

  std::string GetBackupEncryptionToken();
  bool RestoreEncryptionToken(const base::StringPiece& token);

  CycleData GetCycleData();
  EngineData GetEngineData();

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

 private:
  const raw_ptr<VivaldiSyncServiceImpl> sync_service_;
  const raw_ptr<VivaldiAccountManager> account_manager_;

  bool tried_decrypt_ = false;
};
}  // namespace vivaldi

#endif  // SYNC_VIVALDI_SYNC_UI_HELPER_H_