// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTES_NOTE_MODEL_TYPE_PROCESSOR_H_
#define SYNC_NOTES_NOTE_MODEL_TYPE_PROCESSOR_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/sync/engine/model_type_processor.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "sync/notes/synced_note_tracker.h"

namespace vivaldi {
class NotesModel;
}

namespace sync_notes {

class NotesModelObserverImpl;

class NoteModelTypeProcessor : public syncer::ModelTypeProcessor,
                               public syncer::ModelTypeControllerDelegate {
 public:
  NoteModelTypeProcessor();

  NoteModelTypeProcessor(const NoteModelTypeProcessor&) = delete;
  NoteModelTypeProcessor& operator=(const NoteModelTypeProcessor&) = delete;

  ~NoteModelTypeProcessor() override;

  // ModelTypeProcessor implementation.
  void ConnectSync(std::unique_ptr<syncer::CommitQueue> worker) override;
  void DisconnectSync() override;
  void GetLocalChanges(size_t max_entries,
                       GetLocalChangesCallback callback) override;
  void OnCommitCompleted(
      const sync_pb::ModelTypeState& type_state,
      const syncer::CommitResponseDataList& committed_response_list,
      const syncer::FailedCommitResponseDataList& error_response_list) override;
  void OnUpdateReceived(const sync_pb::ModelTypeState& type_state,
                        syncer::UpdateResponseDataList updates) override;

  // ModelTypeControllerDelegate implementation.
  void OnSyncStarting(const syncer::DataTypeActivationRequest& request,
                      StartCallback start_callback) override;
  void OnSyncStopping(syncer::SyncStopMetadataFate metadata_fate) override;
  void GetAllNodesForDebugging(AllNodesCallback callback) override;
  void GetTypeEntitiesCountForDebugging(
      base::OnceCallback<void(const syncer::TypeEntitiesCount&)> callback)
      const override;
  void RecordMemoryUsageAndCountsHistograms() override;

  // Encodes all sync metadata into a string, representing a state that can be
  // restored via ModelReadyToSync() below.
  std::string EncodeSyncMetadata() const;

  // It mainly decodes a NoteModelMetadata proto serialized in
  // |metadata_str|, and uses it to fill in the tracker and the model type state
  // objects. |model| must not be null and must outlive this object. It is used
  // to the retrieve the local node ids, and is stored in the processor to be
  // used for further model operations. |schedule_save_closure| is a repeating
  // closure used to schedule a save of the note model together with the
  // metadata.
  void ModelReadyToSync(const std::string& metadata_str,
                        const base::RepeatingClosure& schedule_save_closure,
                        vivaldi::NotesModel* model);

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

  const SyncedNoteTracker* GetTrackerForTest() const;
  bool IsConnectedForTest() const;

  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetWeakPtr();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // If preconditions are met, inform sync that we are ready to connect.
  void ConnectIfReady();

  // Nudges worker if there are any local entities to be committed. Should only
  // be called after initial sync is done and processor is tracking sync
  // entities.
  void NudgeForCommitIfNeeded();

  // Performs the required clean up when note model is being deleted.
  void OnNotesModelBeingDeleted();

  // Process specifically calls to OnUpdateReceived() that correspond to the
  // initial merge of notes (e.g. was just enabled).
  void OnInitialUpdateReceived(const sync_pb::ModelTypeState& type_state,
                               syncer::UpdateResponseDataList updates);

  // Instantiates the required objects to track metadata and starts observing
  // changes from the note model.
  void StartTrackingMetadata();
  void StopTrackingMetadata();

  // Creates a DictionaryValue for local and remote debugging information about
  // |node| and appends it to |all_nodes|. It does the same for child nodes
  // recursively. |index| is the index of |node| within its parent. |index|
  // could computed from |node|, however it's much cheaper to pass from outside
  // since we iterate over child nodes already in the calling sites.
  void AppendNodeAndChildrenForDebugging(const vivaldi::NoteNode* node,
                                         int index,
                                         base::ListValue* all_nodes) const;

  // Stores the start callback in between OnSyncStarting() and
  // ModelReadyToSync().
  StartCallback start_callback_;

  // The note model we are processing local changes from and forwarding
  // remote changes to. It is set during ModelReadyToSync(), which is called
  // during startup, as part of the note-loading process.
  vivaldi::NotesModel* notes_model_ = nullptr;

  // The callback used to schedule the persistence of note model as well as
  // the metadata to a file during which latest metadata should also be pulled
  // via EncodeSyncMetadata. Processor should invoke it upon changes in the
  // metadata that don't imply changes in the model itself. Persisting updates
  // that imply model changes is the model's responsibility.
  base::RepeatingClosure schedule_save_closure_;

  // Reference to the CommitQueue.
  //
  // The interface hides the posting of tasks across threads as well as the
  // CommitQueue's implementation.  Both of these features are
  // useful in tests.
  std::unique_ptr<syncer::CommitQueue> worker_;

  // Keeps the mapping between server ids and notes nodes together with sync
  // metadata. It is constructed and set during ModelReadyToSync(), if the
  // loaded notes JSON contained previous sync metadata, or upon completion
  // of initial sync, which is called during startup, as part of the
  // note-loading process.
  std::unique_ptr<SyncedNoteTracker> note_tracker_;

  // GUID string that identifies the sync client and is received from the sync
  // engine.
  std::string cache_guid_;

  syncer::ModelErrorHandler error_handler_;

  std::unique_ptr<NotesModelObserverImpl> notes_model_observer_;

  // WeakPtrFactory for this processor for ModelTypeController.
  base::WeakPtrFactory<NoteModelTypeProcessor> weak_ptr_factory_for_controller_{
      this};

  // WeakPtrFactory for this processor which will be sent to sync thread.
  base::WeakPtrFactory<NoteModelTypeProcessor> weak_ptr_factory_for_worker_{
      this};
};

}  // namespace sync_notes

#endif  // SYNC_NOTES_NOTE_MODEL_TYPE_PROCESSOR_H_
