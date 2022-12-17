// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notes/note_model_type_processor.h"

#include <utility>

#include "app/vivaldi_apptools.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/trace_event.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/model_type_processor_proxy.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/type_entities_count.h"
#include "components/sync/protocol/notes_model_metadata.pb.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "notes/note_node.h"
#include "notes/notes_model.h"
#include "sync/notes/note_local_changes_builder.h"
#include "sync/notes/note_model_merger.h"
#include "sync/notes/note_remote_updates_handler.h"
#include "sync/notes/note_specifics_conversions.h"
#include "sync/notes/notes_model_observer_impl.h"
#include "sync/notes/parent_guid_preprocessing.h"
#include "sync/notes/synced_note_tracker_entity.h"
#include "ui/base/models/tree_node_iterator.h"

namespace sync_notes {

namespace {

class ScopedRemoteUpdateNotes {
 public:
  // |notes_model|, and |observer| must not be null
  // and must outlive this object.
  ScopedRemoteUpdateNotes(vivaldi::NotesModel* notes_model,
                          vivaldi::NotesModelObserver* observer)
      : notes_model_(notes_model), observer_(observer) {
    // Notify UI intensive observers of NotesModel that we are about to make
    // potentially significant changes to it, so the updates may be batched.
    DCHECK(notes_model_);
    notes_model_->BeginExtensiveChanges();
    // Shouldn't be notified upon changes due to sync.
    notes_model_->RemoveObserver(observer_);
  }

  ScopedRemoteUpdateNotes(const ScopedRemoteUpdateNotes&) = delete;
  ScopedRemoteUpdateNotes& operator=(const ScopedRemoteUpdateNotes&) = delete;

  ~ScopedRemoteUpdateNotes() {
    // Notify UI intensive observers of NotesModel that all updates have been
    // applied, and that they may now be consumed.
    notes_model_->EndExtensiveChanges();
    notes_model_->AddObserver(observer_);
  }

 private:
  const raw_ptr<vivaldi::NotesModel> notes_model_;

  const raw_ptr<vivaldi::NotesModelObserver> observer_;
};

std::string ComputeServerDefinedUniqueTagForDebugging(
    const vivaldi::NoteNode* node,
    vivaldi::NotesModel* model) {
  if (node == model->main_node()) {
    return "main_notes";
  }
  if (node == model->other_node()) {
    return "other_notes";
  }
  if (node == model->trash_node()) {
    return "trash_notes";
  }
  return "";
}

}  // namespace

NoteModelTypeProcessor::NoteModelTypeProcessor() {}

NoteModelTypeProcessor::~NoteModelTypeProcessor() {
  if (notes_model_ && notes_model_observer_) {
    notes_model_->RemoveObserver(notes_model_observer_.get());
  }
}

void NoteModelTypeProcessor::ConnectSync(
    std::unique_ptr<syncer::CommitQueue> worker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!worker_);
  DCHECK(notes_model_);

  worker_ = std::move(worker);

  // |note_tracker_| is instantiated only after initial sync is done.
  if (note_tracker_) {
    NudgeForCommitIfNeeded();
  }
}

void NoteModelTypeProcessor::DisconnectSync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  weak_ptr_factory_for_worker_.InvalidateWeakPtrs();
  if (!worker_) {
    return;
  }

  DVLOG(1) << "Disconnecting sync for Notes";
  worker_.reset();
}

void NoteModelTypeProcessor::GetLocalChanges(size_t max_entries,
                                             GetLocalChangesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NoteLocalChangesBuilder builder(note_tracker_.get(), notes_model_);
  std::move(callback).Run(builder.BuildCommitRequests(max_entries));
}

void NoteModelTypeProcessor::OnCommitCompleted(
    const sync_pb::ModelTypeState& type_state,
    const syncer::CommitResponseDataList& committed_response_list,
    const syncer::FailedCommitResponseDataList& error_response_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // |error_response_list| is ignored, because all errors are treated as
  // transient and the processor with eventually retry.

  for (const syncer::CommitResponseData& response : committed_response_list) {
    const SyncedNoteTrackerEntity* entity =
        note_tracker_->GetEntityForClientTagHash(response.client_tag_hash);
    if (!entity) {
      DLOG(WARNING) << "Received a commit response for an unknown entity.";
      continue;
    }

    note_tracker_->UpdateUponCommitResponse(entity, response.id,
                                            response.response_version,
                                            response.sequence_number);
  }
  note_tracker_->set_model_type_state(type_state);
  schedule_save_closure_.Run();
}

void NoteModelTypeProcessor::OnUpdateReceived(
    const sync_pb::ModelTypeState& model_type_state,
    syncer::UpdateResponseDataList updates) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!model_type_state.cache_guid().empty());
  DCHECK_EQ(model_type_state.cache_guid(), cache_guid_);
  DCHECK(model_type_state.initial_sync_done());

  // Clients before M94 did not populate the parent GUID in specifics.
  PopulateParentGuidInSpecifics(note_tracker_.get(), &updates);

  if (!note_tracker_) {
    OnInitialUpdateReceived(model_type_state, std::move(updates));
    return;
  }

  // Incremental updates.
  ScopedRemoteUpdateNotes update_notes(notes_model_,
                                       notes_model_observer_.get());
  NoteRemoteUpdatesHandler updates_handler(notes_model_, note_tracker_.get());
  const bool got_new_encryption_requirements =
      note_tracker_->model_type_state().encryption_key_name() !=
      model_type_state.encryption_key_name();
  note_tracker_->set_model_type_state(model_type_state);
  updates_handler.Process(updates, got_new_encryption_requirements);
  if (note_tracker_->ReuploadNotesOnLoadIfNeeded()) {
    NudgeForCommitIfNeeded();
  }
  // There are cases when we receive non-empty updates that don't result in
  // model changes (e.g. reflections). In that case, issue a write to persit the
  // progress marker in order to avoid downloading those updates again.
  if (!updates.empty()) {
    // Schedule save just in case one is needed.
    schedule_save_closure_.Run();
  }
}

const SyncedNoteTracker* NoteModelTypeProcessor::GetTrackerForTest() const {
  return note_tracker_.get();
}

bool NoteModelTypeProcessor::IsConnectedForTest() const {
  return worker_ != nullptr;
}

std::string NoteModelTypeProcessor::EncodeSyncMetadata() const {
  std::string metadata_str;
  if (note_tracker_) {
    sync_pb::NotesModelMetadata model_metadata =
        note_tracker_->BuildNoteModelMetadata();
    model_metadata.SerializeToString(&metadata_str);
  }
  return metadata_str;
}

void NoteModelTypeProcessor::ModelReadyToSync(
    const std::string& metadata_str,
    const base::RepeatingClosure& schedule_save_closure,
    vivaldi::NotesModel* model) {
  DCHECK(model);
  DCHECK(model->loaded());
  DCHECK(!notes_model_);
  DCHECK(!note_tracker_);
  DCHECK(!notes_model_observer_);

  // TODO(crbug.com/950869): Remove after investigations are completed.
  TRACE_EVENT0("browser", "NoteModelTypeProcessor::ModelReadyToSync");

  notes_model_ = model;
  schedule_save_closure_ = schedule_save_closure;

  sync_pb::NotesModelMetadata model_metadata;
  model_metadata.ParseFromString(metadata_str);

  note_tracker_ = SyncedNoteTracker::CreateFromNotesModelAndMetadata(
      model, std::move(model_metadata));

  if (note_tracker_) {
    StartTrackingMetadata();
  } else if (!metadata_str.empty()) {
    DLOG(WARNING) << "Persisted note sync metadata invalidated when loading.";
    // Schedule a save to make sure the corrupt metadata is deleted from disk as
    // soon as possible, to avoid reporting again after restart if nothing else
    // schedules a save meanwhile (which is common if sync is not running
    // properly, e.g. auth error).
    schedule_save_closure_.Run();
  }

  ConnectIfReady();
}

size_t NoteModelTypeProcessor::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  size_t memory_usage = 0;
  if (note_tracker_) {
    memory_usage += note_tracker_->EstimateMemoryUsage();
  }
  memory_usage += EstimateMemoryUsage(cache_guid_);
  return memory_usage;
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
NoteModelTypeProcessor::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_for_controller_.GetWeakPtr();
}

void NoteModelTypeProcessor::OnSyncStarting(
    const syncer::DataTypeActivationRequest& request,
    StartCallback start_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(start_callback);
  DVLOG(1) << "Sync is starting for Notes";

  cache_guid_ = request.cache_guid;
  start_callback_ = std::move(start_callback);
  error_handler_ = request.error_handler;

  DCHECK(!cache_guid_.empty());
  ConnectIfReady();
}

void NoteModelTypeProcessor::ConnectIfReady() {
  // Return if the model isn't ready.
  if (!notes_model_) {
    return;
  }
  // Return if Sync didn't start yet.
  if (!start_callback_) {
    return;
  }

  DCHECK(!cache_guid_.empty());

  if (note_tracker_ &&
      note_tracker_->model_type_state().cache_guid() != cache_guid_) {
    // TODO(crbug.com/820049): Add basic unit testing  consider using
    // StopTrackingMetadata().
    // In case of a cache guid mismatch, treat it as a corrupted metadata and
    // start clean.
    notes_model_->RemoveObserver(notes_model_observer_.get());
    notes_model_observer_.reset();
    note_tracker_.reset();
  }

  auto activation_context =
      std::make_unique<syncer::DataTypeActivationResponse>();
  if (note_tracker_) {
    activation_context->model_type_state = note_tracker_->model_type_state();
  } else {
    sync_pb::ModelTypeState model_type_state;
    model_type_state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromModelType(syncer::NOTES));
    model_type_state.set_cache_guid(cache_guid_);
    activation_context->model_type_state = model_type_state;
  }
  activation_context->type_processor =
      std::make_unique<syncer::ModelTypeProcessorProxy>(
          weak_ptr_factory_for_worker_.GetWeakPtr(),
          base::SequencedTaskRunnerHandle::Get());
  std::move(start_callback_).Run(std::move(activation_context));
}

void NoteModelTypeProcessor::OnSyncStopping(
    syncer::SyncStopMetadataFate metadata_fate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Disabling sync for a type shouldn't happen before the model is loaded
  // because OnSyncStopping() is not allowed to be called before
  // OnSyncStarting() has completed..
  DCHECK(notes_model_);
  DCHECK(!start_callback_);

  cache_guid_.clear();
  worker_.reset();

  switch (metadata_fate) {
    case syncer::KEEP_METADATA: {
      break;
    }

    case syncer::CLEAR_METADATA: {
      // Stop observing local changes. We'll start observing local changes again
      // when Sync is (re)started in StartTrackingMetadata().
      if (note_tracker_) {
        DCHECK(notes_model_observer_);
        notes_model_->RemoveObserver(notes_model_observer_.get());
        notes_model_observer_.reset();
        note_tracker_.reset();
      }
      schedule_save_closure_.Run();
      break;
    }
  }

  // Do not let any delayed callbacks to be called.
  weak_ptr_factory_for_controller_.InvalidateWeakPtrs();
  weak_ptr_factory_for_worker_.InvalidateWeakPtrs();
}

void NoteModelTypeProcessor::NudgeForCommitIfNeeded() {
  DCHECK(note_tracker_);
  // Don't bother sending anything if there's no one to send to.
  if (!worker_) {
    return;
  }

  // Nudge worker if there are any entities with local changes.
  if (note_tracker_->HasLocalChanges()) {
    worker_->NudgeForCommit();
  }
}

void NoteModelTypeProcessor::OnNotesModelBeingDeleted() {
  DCHECK(notes_model_);
  DCHECK(notes_model_observer_);
  StopTrackingMetadata();
}

void NoteModelTypeProcessor::OnInitialUpdateReceived(
    const sync_pb::ModelTypeState& model_type_state,
    syncer::UpdateResponseDataList updates) {
  DCHECK(!note_tracker_);

  TRACE_EVENT0("sync", "NoteModelTypeProcessor::OnInitialUpdateReceived");

  note_tracker_ = SyncedNoteTracker::CreateEmpty(model_type_state);
  StartTrackingMetadata();

  {
    ScopedRemoteUpdateNotes update_notes(notes_model_,
                                         notes_model_observer_.get());

    NoteModelMerger(std::move(updates), notes_model_, note_tracker_.get())
        .Merge();
  }

  // If any of the permanent nodes is missing, we treat it as failure.
  if (!note_tracker_->GetEntityForNoteNode(notes_model_->main_node()) ||
      !note_tracker_->GetEntityForNoteNode(notes_model_->other_node()) ||
      !note_tracker_->GetEntityForNoteNode(notes_model_->trash_node())) {
    StopTrackingMetadata();
    note_tracker_.reset();
    error_handler_.Run(
        syncer::ModelError(FROM_HERE, "Permanent note entities missing"));
    return;
  }

  note_tracker_->CheckAllNodesTracked(notes_model_);

  schedule_save_closure_.Run();
  NudgeForCommitIfNeeded();
}

void NoteModelTypeProcessor::StartTrackingMetadata() {
  DCHECK(note_tracker_);
  DCHECK(!notes_model_observer_);

  notes_model_observer_ = std::make_unique<NotesModelObserverImpl>(
      base::BindRepeating(&NoteModelTypeProcessor::NudgeForCommitIfNeeded,
                          base::Unretained(this)),
      base::BindOnce(&NoteModelTypeProcessor::OnNotesModelBeingDeleted,
                     base::Unretained(this)),
      note_tracker_.get());
  notes_model_->AddObserver(notes_model_observer_.get());
}

void NoteModelTypeProcessor::StopTrackingMetadata() {
  DCHECK(notes_model_observer_);

  notes_model_->RemoveObserver(notes_model_observer_.get());
  notes_model_ = nullptr;
  notes_model_observer_.reset();

  DisconnectSync();
}

void NoteModelTypeProcessor::GetAllNodesForDebugging(
    AllNodesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::List all_nodes;
  // Create a permanent folder since sync server no longer create root folders,
  // and USS won't migrate root folders from directory, we create root folders.
  base::Value::Dict root_node;
  // Function isTypeRootNode in sync_node_browser.js use PARENT_ID and
  // UNIQUE_SERVER_TAG to check if the node is root node. isChildOf in
  // sync_node_browser.js uses modelType to check if root node is parent of real
  // data node. NON_UNIQUE_NAME will be the name of node to display.
  root_node.Set("ID", "NOTES_ROOT");
  root_node.Set("PARENT_ID", "r");
  root_node.Set("UNIQUE_SERVER_TAG", "vivaldi_notes");
  root_node.Set("IS_DIR", true);
  root_node.Set("modelType", "Notes");
  root_node.Set("NON_UNIQUE_NAME", "Notes");
  all_nodes.Append(std::move(root_node));

  const vivaldi::NoteNode* model_root_node = notes_model_->root_node();
  int i = 0;
  for (const auto& child : model_root_node->children()) {
    AppendNodeAndChildrenForDebugging(child.get(), i++, &all_nodes);
  }

  std::move(callback).Run(syncer::NOTES, std::move(all_nodes));
}

void NoteModelTypeProcessor::AppendNodeAndChildrenForDebugging(
    const vivaldi::NoteNode* node,
    int index,
    base::Value::List* all_nodes) const {
  const SyncedNoteTrackerEntity* entity =
      note_tracker_->GetEntityForNoteNode(node);
  // Include only tracked nodes. Newly added nodes are tracked even before being
  // sent to the server.
  if (!entity) {
    return;
  }
  const sync_pb::EntityMetadata& metadata = entity->metadata();
  // Copy data to an EntityData object to reuse its conversion
  // ToDictionaryValue() methods.
  syncer::EntityData data;
  data.id = metadata.server_id();
  data.creation_time = node->GetCreationTime();
  data.modification_time =
      syncer::ProtoTimeToTime(metadata.modification_time());
  data.name = base::UTF16ToUTF8(node->GetTitle().empty() ? node->GetContent()
                                                         : node->GetTitle());
  data.specifics = CreateSpecificsFromNoteNode(node, notes_model_,
                                               metadata.unique_position());
  if (node->is_permanent_node()) {
    data.server_defined_unique_tag =
        ComputeServerDefinedUniqueTagForDebugging(node, notes_model_);
    // Set the parent to empty string to indicate it's parent of the root node
    // for notes. The code in sync_node_browser.js links nodes with the
    // "modelType" when they are lacking a parent id.
    data.legacy_parent_id = "";
  } else {
    const vivaldi::NoteNode* parent = node->parent();
    const SyncedNoteTrackerEntity* parent_entity =
        note_tracker_->GetEntityForNoteNode(parent);
    DCHECK(parent_entity);
    data.legacy_parent_id = parent_entity->metadata().server_id();
  }

  base::Value::Dict data_dictionary = data.ToDictionaryValue();
  // Set ID value as in legacy directory-based implementation, "s" means server.
  data_dictionary.Set("ID", "s" + metadata.server_id());
  if (node->is_permanent_node()) {
    // Hardcode the parent of permanent nodes.
    data_dictionary.Set("PARENT_ID", "NOTES_ROOT");
    data_dictionary.Set("UNIQUE_SERVER_TAG", data.server_defined_unique_tag);
  } else {
    data_dictionary.Set("PARENT_ID", "s" + data.legacy_parent_id);
  }
  data_dictionary.Set("LOCAL_EXTERNAL_ID", static_cast<int>(node->id()));
  data_dictionary.Set("positionIndex", index);
  data_dictionary.Set("metadata", base::Value::FromUniquePtrValue(
                                      syncer::EntityMetadataToValue(metadata)));
  data_dictionary.Set("modelType", "Notes");
  data_dictionary.Set("IS_DIR", node->is_folder());
  all_nodes->Append(std::move(data_dictionary));

  int i = 0;
  for (const auto& child : node->children()) {
    AppendNodeAndChildrenForDebugging(child.get(), i++, all_nodes);
  }
}

void NoteModelTypeProcessor::GetTypeEntitiesCountForDebugging(
    base::OnceCallback<void(const syncer::TypeEntitiesCount&)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  syncer::TypeEntitiesCount count(syncer::NOTES);
  if (note_tracker_) {
    count.non_tombstone_entities = note_tracker_->TrackedNotesCount();
    count.entities = count.non_tombstone_entities +
                     note_tracker_->TrackedUncommittedTombstonesCount();
  }
  std::move(callback).Run(count);
}

void NoteModelTypeProcessor::RecordMemoryUsageAndCountsHistograms() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SyncRecordModelTypeMemoryHistogram(syncer::NOTES, EstimateMemoryUsage());
  if (note_tracker_) {
    SyncRecordModelTypeCountHistogram(syncer::NOTES,
                                      note_tracker_->TrackedNotesCount());
  } else {
    SyncRecordModelTypeCountHistogram(syncer::NOTES, 0);
  }
}

}  // namespace sync_notes
