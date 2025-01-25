// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notes/note_data_type_processor.h"

#include <utility>
#include <vector>

#include "app/vivaldi_apptools.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/notes/note_node.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/data_type_processor_proxy.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/type_entities_count.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/data_type_state_helper.h"
#include "components/sync/protocol/notes_model_metadata.pb.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "sync/file_sync/file_store.h"
#include "sync/notes/note_local_changes_builder.h"
#include "sync/notes/note_model_merger.h"
#include "sync/notes/note_model_view.h"
#include "sync/notes/note_remote_updates_handler.h"
#include "sync/notes/note_specifics_conversions.h"
#include "sync/notes/notes_model_observer_impl.h"
#include "sync/notes/parent_guid_preprocessing.h"
#include "sync/notes/synced_note_tracker_entity.h"
#include "ui/base/models/tree_node_iterator.h"

namespace sync_notes {

namespace {

constexpr size_t kDefaultMaxNotesTillSyncEnabled = 100000;

class ScopedRemoteUpdateNotes {
 public:
  // `notes_model`, and `observer` must not be null
  // and must outlive this object.
  ScopedRemoteUpdateNotes(NoteModelView* notes_model,
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
  const raw_ptr<NoteModelView> notes_model_;

  const raw_ptr<vivaldi::NotesModelObserver> observer_;
};

std::string ComputeServerDefinedUniqueTagForDebugging(
    const vivaldi::NoteNode* node,
    NoteModelView* model) {
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

size_t CountSyncableNotesFromModel(NoteModelView* model) {
  size_t count = 0;
  ui::TreeNodeIterator<const vivaldi::NoteNode> iterator(model->root_node());
  // Does not count the root node.
  while (iterator.has_next()) {
    const vivaldi::NoteNode* node = iterator.Next();
    if (model->IsNodeSyncable(node)) {
      ++count;
    }
  }
  return count;
}

}  // namespace

NoteDataTypeProcessor::NoteDataTypeProcessor(
    file_sync::SyncedFileStore* synced_file_store,
    syncer::WipeModelUponSyncDisabledBehavior
        wipe_model_upon_sync_disabled_behavior)
    : synced_file_store_(synced_file_store),
      wipe_model_upon_sync_disabled_behavior_(
          wipe_model_upon_sync_disabled_behavior),
      max_notes_till_sync_enabled_(kDefaultMaxNotesTillSyncEnabled) {}

NoteDataTypeProcessor::~NoteDataTypeProcessor() {
  if (notes_model_ && notes_model_observer_) {
    notes_model_->RemoveObserver(notes_model_observer_.get());
  }
}

void NoteDataTypeProcessor::ConnectSync(
    std::unique_ptr<syncer::CommitQueue> worker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!worker_);
  DCHECK(notes_model_);

  worker_ = std::move(worker);

  // `note_tracker_` is instantiated only after initial sync is done.
  if (note_tracker_) {
    NudgeForCommitIfNeeded();
  }
}

void NoteDataTypeProcessor::DisconnectSync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  weak_ptr_factory_for_worker_.InvalidateWeakPtrs();
  if (!worker_) {
    return;
  }

  DVLOG(1) << "Disconnecting sync for Notes";
  worker_.reset();
}

void NoteDataTypeProcessor::GetLocalChanges(size_t max_entries,
                                            GetLocalChangesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Processor should never connect if
  // `last_initial_merge_remote_updates_exceeded_limit_` is set.
  DCHECK(!last_initial_merge_remote_updates_exceeded_limit_);
  NoteLocalChangesBuilder builder(note_tracker_.get(), notes_model_);
  std::move(callback).Run(builder.BuildCommitRequests(max_entries));
}

void NoteDataTypeProcessor::OnCommitCompleted(
    const sync_pb::DataTypeState& type_state,
    const syncer::CommitResponseDataList& committed_response_list,
    const syncer::FailedCommitResponseDataList& error_response_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // `error_response_list` is ignored, because all errors are treated as
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
  note_tracker_->set_data_type_state(type_state);
  schedule_save_closure_.Run();
}

void NoteDataTypeProcessor::OnUpdateReceived(
    const sync_pb::DataTypeState& data_type_state,
    syncer::UpdateResponseDataList updates,
    std::optional<sync_pb::GarbageCollectionDirective> gc_directive) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!data_type_state.cache_guid().empty());
  CHECK_EQ(data_type_state.cache_guid(), activation_request_.cache_guid);
  DCHECK(syncer::IsInitialSyncDone(data_type_state.initial_sync_state()));
  DCHECK(start_callback_.is_null());
  // Processor should never connect if
  // `last_initial_merge_remote_updates_exceeded_limit_` is set.
  DCHECK(!last_initial_merge_remote_updates_exceeded_limit_);

  // TODO(crbug.com/40860698): validate incoming updates, e.g. `gc_directive`
  // must be empty for Notes.

  // Clients before M94 did not populate the parent UUID in specifics.
  PopulateParentGuidInSpecifics(note_tracker_.get(), &updates);

  if (!note_tracker_) {
    OnInitialUpdateReceived(data_type_state, std::move(updates));
    return;
  }

  // Incremental updates.
  {
    ScopedRemoteUpdateNotes update_notess(notes_model_,
                                          notes_model_observer_.get());
    NoteRemoteUpdatesHandler updates_handler(notes_model_, note_tracker_.get());
    const bool got_new_encryption_requirements =
        note_tracker_->data_type_state().encryption_key_name() !=
        data_type_state.encryption_key_name();
    note_tracker_->set_data_type_state(data_type_state);
    updates_handler.Process(updates, got_new_encryption_requirements);
  }

  // Issue error and stop sync if notes count exceeds limit.
  if (note_tracker_->TrackedNotesCount() > max_notes_till_sync_enabled_) {
    // Local changes continue to be tracked in order to allow users to delete
    // notes and recover upon restart.
    DisconnectSync();
    activation_request_.error_handler.Run(
        syncer::ModelError(FROM_HERE, "Local notes count exceed limit."));
    return;
  }

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

void NoteDataTypeProcessor::StorePendingInvalidations(
    std::vector<sync_pb::DataTypeState::Invalidation> invalidations_to_store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!note_tracker_) {
    // It's possible to receive invalidations while notes are not syncing,
    // e.g. if invalidation system is initialized earlier than note model.
    return;
  }
  sync_pb::DataTypeState data_type_state = note_tracker_->data_type_state();
  data_type_state.mutable_invalidations()->Assign(
      invalidations_to_store.begin(), invalidations_to_store.end());
  note_tracker_->set_data_type_state(data_type_state);
  schedule_save_closure_.Run();
}

bool NoteDataTypeProcessor::IsTrackingMetadata() const {
  return note_tracker_.get() != nullptr;
}

const SyncedNoteTracker* NoteDataTypeProcessor::GetTrackerForTest() const {
  return note_tracker_.get();
}

bool NoteDataTypeProcessor::IsConnectedForTest() const {
  return worker_ != nullptr;
}

std::string NoteDataTypeProcessor::EncodeSyncMetadata() const {
  std::string metadata_str;
  if (note_tracker_) {
    // `last_initial_merge_remote_updates_exceeded_limit_` is only set in error
    // cases where the tracker would not be initialized.
    DCHECK(!last_initial_merge_remote_updates_exceeded_limit_);

    sync_pb::NotesModelMetadata model_metadata =
        note_tracker_->BuildNoteModelMetadata();
    // Ensure that BuildNoteModelMetadata() never populates this field.
    DCHECK(
        !model_metadata.has_last_initial_merge_remote_updates_exceeded_limit());
    model_metadata.SerializeToString(&metadata_str);
  } else if (last_initial_merge_remote_updates_exceeded_limit_) {
    sync_pb::NotesModelMetadata model_metadata;
    // Setting the field only when true guarantees that the empty-string case
    // is interpreted as no-metadata-to-clear.
    model_metadata.set_last_initial_merge_remote_updates_exceeded_limit(true);
    model_metadata.SerializeToString(&metadata_str);
  }
  return metadata_str;
}

void NoteDataTypeProcessor::ModelReadyToSync(
    const std::string& metadata_str,
    const base::RepeatingClosure& schedule_save_closure,
    NoteModelView* model) {
  DCHECK(model);
  DCHECK(model->loaded());
  DCHECK(!notes_model_);
  DCHECK(!note_tracker_);
  DCHECK(!notes_model_observer_);

  // TODO(crbug.com/950869): Remove after investigations are completed.
  TRACE_EVENT0("sync", "NoteDataTypeProcessor::ModelReadyToSync");

  notes_model_ = model;
  schedule_save_closure_ = schedule_save_closure;

  sync_pb::NotesModelMetadata model_metadata;
  model_metadata.ParseFromString(metadata_str);

  if (pending_clear_metadata_) {
    pending_clear_metadata_ = false;
    // Schedule save empty metadata, if not already empty.
    if (!metadata_str.empty()) {
      if (syncer::IsInitialSyncDone(
              model_metadata.data_type_state().initial_sync_state())) {
        // There used to be a tracker, which is dropped now due to
        // `pending_clear_metadata_`. This isn't very different to
        // ClearMetadataIfStopped(), in the sense that the need to wipe the
        // local model needs to be considered.
        TriggerWipeModelUponSyncDisabledBehavior();
      }
      schedule_save_closure_.Run();
    }
  } else if (model_metadata
                 .last_initial_merge_remote_updates_exceeded_limit()) {
    // Report error if remote updates fetched last time during initial merge
    // exceeded limit. Note that here we are only setting
    // `last_initial_merge_remote_updates_exceeded_limit_`, the actual error
    // would be reported in ConnectIfReady().
    last_initial_merge_remote_updates_exceeded_limit_ = true;
  } else {
    note_tracker_ = SyncedNoteTracker::CreateFromNotesModelAndMetadata(
        model, std::move(model_metadata), synced_file_store_);

    if (note_tracker_) {
      StartTrackingMetadata();
    } else if (!metadata_str.empty()) {
      DLOG(WARNING) << "Persisted note sync metadata invalidated when loading.";
      // Schedule a save to make sure the corrupt metadata is deleted from disk
      // as soon as possible, to avoid reporting again after restart if nothing
      // else schedules a save meanwhile (which is common if sync is not running
      // properly, e.g. auth error).
      schedule_save_closure_.Run();
    }
  }

  if (!note_tracker_) {
    switch (wipe_model_upon_sync_disabled_behavior_) {
      case syncer::WipeModelUponSyncDisabledBehavior::kNever:
        // Nothing to do.
        break;
      case syncer::WipeModelUponSyncDisabledBehavior::kOnceIfTrackingMetadata:
        // Since the model isn't initially tracking metadata, move away from
        // kOnceIfTrackingMetadata so the behavior doesn't kick in, in case sync
        // is turned on later and back to off. This should be practically
        // unreachable because usually ClearMetadataIfStopped() would be invoked
        // earlier, but let's be extra safe and avoid relying on this behavior.
        wipe_model_upon_sync_disabled_behavior_ =
            syncer::WipeModelUponSyncDisabledBehavior::kNever;
        break;
      case syncer::WipeModelUponSyncDisabledBehavior::kAlways:
        // Remove any previous data that may exist, if its lifetime is strongly
        // coupled with the tracker's (sync metadata's).
        notes_model_->RemoveAllSyncableNodes();
        break;
    }
  }

  ConnectIfReady();
}

size_t NoteDataTypeProcessor::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  size_t memory_usage = 0;
  if (note_tracker_) {
    memory_usage += note_tracker_->EstimateMemoryUsage();
  }
  memory_usage += EstimateMemoryUsage(activation_request_.cache_guid);
  return memory_usage;
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
NoteDataTypeProcessor::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_for_controller_.GetWeakPtr();
}

void NoteDataTypeProcessor::OnSyncStarting(
    const syncer::DataTypeActivationRequest& request,
    StartCallback start_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(start_callback);
  CHECK(request.IsValid());
  CHECK(!request.cache_guid.empty());
  DVLOG(1) << "Sync is starting for Notes";

  start_callback_ = std::move(start_callback);
  activation_request_ = request;

  ConnectIfReady();
}

void NoteDataTypeProcessor::ConnectIfReady() {
  // Return if the model isn't ready.
  if (!notes_model_) {
    return;
  }
  // Return if Sync didn't start yet.
  if (!start_callback_) {
    return;
  }

  // ConnectSync() should not have been called by now.
  DCHECK(!worker_);

  // Report error if remote updates fetched last time during initial merge
  // exceeded limit.
  if (last_initial_merge_remote_updates_exceeded_limit_) {
    // `last_initial_merge_remote_updates_exceeded_limit_` is only set in error
    // case and thus tracker should be empty.
    DCHECK(!note_tracker_);
    start_callback_.Reset();
    activation_request_.error_handler.Run(
        syncer::ModelError(FROM_HERE,
                           "Latest remote note count exceeded limit. Turn "
                           "off and turn on sync to retry."));
    return;
  }

  // Issue error and stop sync if notes exceed limit.
  // TODO(crbug.com/40854724): Think about adding two different limits: one for
  // when sync just starts, the other (larger one) as hard limit, incl.
  // incremental changes.
  const size_t count = note_tracker_
                           ? note_tracker_->TrackedNotesCount()
                           : CountSyncableNotesFromModel(notes_model_);
  if (count > max_notes_till_sync_enabled_) {
    // For the case where a tracker already exists, local changes will continue
    // to be tracked in order order to allow users to delete notes and
    // recover upon restart.
    start_callback_.Reset();
    activation_request_.error_handler.Run(
        syncer::ModelError(FROM_HERE, "Local notes count exceed limit."));
    return;
  }

  DCHECK(!activation_request_.cache_guid.empty());

  if (note_tracker_ && note_tracker_->data_type_state().cache_guid() !=
                           activation_request_.cache_guid) {
    // In case of a cache uuid mismatch, treat it as a corrupted metadata and
    // start clean.
    StopTrackingMetadataAndResetTracker();
  }

  auto activation_context =
      std::make_unique<syncer::DataTypeActivationResponse>();
  if (note_tracker_) {
    activation_context->data_type_state = note_tracker_->data_type_state();
  } else {
    sync_pb::DataTypeState data_type_state;
    data_type_state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromDataType(syncer::NOTES));
    data_type_state.set_cache_guid(activation_request_.cache_guid);
    activation_context->data_type_state = data_type_state;
  }
  activation_context->type_processor =
      std::make_unique<syncer::DataTypeProcessorProxy>(
          weak_ptr_factory_for_worker_.GetWeakPtr(),
          base::SequencedTaskRunner::GetCurrentDefault());
  std::move(start_callback_).Run(std::move(activation_context));
}

void NoteDataTypeProcessor::OnSyncStopping(
    syncer::SyncStopMetadataFate metadata_fate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Disabling sync for a type shouldn't happen before the model is loaded
  // because OnSyncStopping() is not allowed to be called before
  // OnSyncStarting() has completed..
  DCHECK(notes_model_);
  DCHECK(!start_callback_);

  activation_request_ = syncer::DataTypeActivationRequest{};

  worker_.reset();

  switch (metadata_fate) {
    case syncer::KEEP_METADATA: {
      break;
    }

    case syncer::CLEAR_METADATA: {
      // Stop observing local changes. We'll start observing local changes again
      // when Sync is (re)started in StartTrackingMetadata(). This is only
      // necessary if a tracker exists, which also means local changes are being
      // tracked (see StartTrackingMetadata()).
      if (note_tracker_) {
        StopTrackingMetadataAndResetTracker();
      }
      last_initial_merge_remote_updates_exceeded_limit_ = false;
      schedule_save_closure_.Run();
      synced_file_store_->RemoveAllSyncRefsForType(syncer::NOTES);
      break;
    }
  }

  // Do not let any delayed callbacks to be called.
  weak_ptr_factory_for_controller_.InvalidateWeakPtrs();
  weak_ptr_factory_for_worker_.InvalidateWeakPtrs();
}

void NoteDataTypeProcessor::NudgeForCommitIfNeeded() {
  DCHECK(note_tracker_);

  // Issue error and stop sync if the number of local notes exceed limit.
  // If `activation_request_.error_handler` is not set, the check is ignored
  // because this gets re-evaluated in ConnectIfReady().
  if (activation_request_.error_handler &&
      note_tracker_->TrackedNotesCount() > max_notes_till_sync_enabled_) {
    // Local changes continue to be tracked in order to allow users to delete
    // notes and recover upon restart.
    DisconnectSync();
    start_callback_.Reset();
    activation_request_.error_handler.Run(
        syncer::ModelError(FROM_HERE, "Local notes count exceed limit."));
    return;
  }

  // Don't bother sending anything if there's no one to send to.
  if (!worker_) {
    return;
  }

  // Nudge worker if there are any entities with local changes.
  if (note_tracker_->HasLocalChanges()) {
    worker_->NudgeForCommit();
  }
}

void NoteDataTypeProcessor::OnNotesModelBeingDeleted() {
  DCHECK(notes_model_);
  DCHECK(notes_model_observer_);

  notes_model_->RemoveObserver(notes_model_observer_.get());
  notes_model_ = nullptr;
  notes_model_observer_.reset();

  DisconnectSync();
}

void NoteDataTypeProcessor::OnInitialUpdateReceived(
    const sync_pb::DataTypeState& data_type_state,
    syncer::UpdateResponseDataList updates) {
  DCHECK(!note_tracker_);
  DCHECK(activation_request_.error_handler);

  TRACE_EVENT0("sync", "NoteDataTypeProcessor::OnInitialUpdateReceived");

  // `updates` can contain an additional root folder. The server may or may not
  // deliver a root node - it is not guaranteed, but this works as an
  // approximated safeguard.
  const size_t max_initial_updates_count = max_notes_till_sync_enabled_ + 1;

  // Report error if count of remote updates is more than the limit.
  // Note that we are not having this check for incremental updates as it is
  // very unlikely that there will be many updates downloaded.
  if (updates.size() > max_initial_updates_count) {
    DisconnectSync();
    last_initial_merge_remote_updates_exceeded_limit_ = true;
    activation_request_.error_handler.Run(
        syncer::ModelError(FROM_HERE, "Remote notes count exceed limit."));
    schedule_save_closure_.Run();
    return;
  }

  note_tracker_ =
      SyncedNoteTracker::CreateEmpty(data_type_state, synced_file_store_);
  StartTrackingMetadata();

  {
    ScopedRemoteUpdateNotes update_notes(notes_model_,
                                         notes_model_observer_.get());

    notes_model_->EnsurePermanentNodesExist();
    NoteModelMerger(std::move(updates), notes_model_, note_tracker_.get())
        .Merge();
  }

  // If any of the permanent nodes is missing, we treat it as failure.
  if (!note_tracker_->GetEntityForNoteNode(notes_model_->main_node()) ||
      !note_tracker_->GetEntityForNoteNode(notes_model_->other_node()) ||
      !note_tracker_->GetEntityForNoteNode(notes_model_->trash_node())) {
    DisconnectSync();
    StopTrackingMetadataAndResetTracker();
    activation_request_.error_handler.Run(
        syncer::ModelError(FROM_HERE, "Permanent note entities missing"));
    return;
  }

  note_tracker_->CheckAllNodesTracked(notes_model_);

  schedule_save_closure_.Run();
  NudgeForCommitIfNeeded();
}

void NoteDataTypeProcessor::StartTrackingMetadata() {
  DCHECK(note_tracker_);
  DCHECK(!notes_model_observer_);

  notes_model_observer_ = std::make_unique<NotesModelObserverImpl>(
      notes_model_,
      base::BindRepeating(&NoteDataTypeProcessor::NudgeForCommitIfNeeded,
                          base::Unretained(this)),
      base::BindOnce(&NoteDataTypeProcessor::OnNotesModelBeingDeleted,
                     base::Unretained(this)),
      note_tracker_.get());
  notes_model_->AddObserver(notes_model_observer_.get());
}

void NoteDataTypeProcessor::HasUnsyncedData(
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(note_tracker_ && note_tracker_->HasLocalChanges());
}

void NoteDataTypeProcessor::GetAllNodesForDebugging(AllNodesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(notes_model_);

  base::Value::List all_nodes;

  // Create a permanent folder since sync server no longer create root folders,
  // and USS won't migrate root folders from directory, we create root folders.

  // Function isTypeRootNode in sync_node_browser.js use PARENT_ID and
  // UNIQUE_SERVER_TAG to check if the node is root node. isChildOf in
  // sync_node_browser.js uses dataType to check if root node is parent of real
  // data node. NON_UNIQUE_NAME will be the name of node to display.
  auto root_node = base::Value::Dict()
                       .Set("ID", "NOTES_ROOT")
                       .Set("PARENT_ID", "r")
                       .Set("UNIQUE_SERVER_TAG", "vivaldi_notes")
                       .Set("IS_DIR", true)
                       .Set("dataType", "Notes")
                       .Set("NON_UNIQUE_NAME", "Notes");
  all_nodes.Append(std::move(root_node));

  const vivaldi::NoteNode* model_root_node = notes_model_->root_node();
  int i = 0;
  for (const auto& child : model_root_node->children()) {
    if (notes_model_->IsNodeSyncable(child.get())) {
      AppendNodeAndChildrenForDebugging(child.get(), i++, &all_nodes);
    }
  }

  std::move(callback).Run(std::move(all_nodes));
}

void NoteDataTypeProcessor::AppendNodeAndChildrenForDebugging(
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
  data.modification_time = node->GetLastModificationTime();
  data.name = base::UTF16ToUTF8(node->GetTitle().empty() ? node->GetContent()
                                                         : node->GetTitle());
  data.specifics = CreateSpecificsFromNoteNode(node, notes_model_,
                                               metadata.unique_position());
  if (node->is_permanent_node()) {
    data.server_defined_unique_tag =
        ComputeServerDefinedUniqueTagForDebugging(node, notes_model_);
    // Set the parent to empty string to indicate it's parent of the root node
    // for notes. The code in sync_node_browser.js links nodes with the
    // "dataType" when they are lacking a parent id.
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
  data_dictionary.Set("metadata", syncer::EntityMetadataToValue(metadata));
  data_dictionary.Set("dataType", "Notes");
  data_dictionary.Set("IS_DIR", node->is_folder() || node->is_note());
  all_nodes->Append(std::move(data_dictionary));

  int i = 0;
  for (const auto& child : node->children()) {
    AppendNodeAndChildrenForDebugging(child.get(), i++, all_nodes);
  }
}

void NoteDataTypeProcessor::GetTypeEntitiesCountForDebugging(
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

void NoteDataTypeProcessor::RecordMemoryUsageAndCountsHistograms() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SyncRecordDataTypeMemoryHistogram(syncer::NOTES, EstimateMemoryUsage());
  if (note_tracker_) {
    SyncRecordDataTypeCountHistogram(syncer::NOTES,
                                     note_tracker_->TrackedNotesCount());
  } else {
    SyncRecordDataTypeCountHistogram(syncer::NOTES, 0);
  }
}

void NoteDataTypeProcessor::SetMaxNotesTillSyncEnabledForTest(size_t limit) {
  max_notes_till_sync_enabled_ = limit;
}

void NoteDataTypeProcessor::ClearMetadataIfStopped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If Sync is not actually stopped, ignore this call.
  if (!activation_request_.cache_guid.empty()) {
    return;
  }

  if (!notes_model_) {
    // Defer the clearing until ModelReadyToSync() is invoked.
    pending_clear_metadata_ = true;
    return;
  }
  if (note_tracker_) {
    StopTrackingMetadataAndResetTracker();
    // Schedule save empty metadata.
    schedule_save_closure_.Run();
  } else if (last_initial_merge_remote_updates_exceeded_limit_) {
    last_initial_merge_remote_updates_exceeded_limit_ = false;
    // Schedule save empty metadata.
    schedule_save_closure_.Run();
  }
}

void NoteDataTypeProcessor::ReportBridgeErrorForTest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DisconnectSync();
  activation_request_.error_handler.Run(
      syncer::ModelError(FROM_HERE, "Report error for test"));
}

void NoteDataTypeProcessor::StopTrackingMetadataAndResetTracker() {
  // DisconnectSync() should have been called by the caller.
  DCHECK(!worker_);
  DCHECK(note_tracker_);
  DCHECK(notes_model_observer_);
  notes_model_->RemoveObserver(notes_model_observer_.get());
  notes_model_observer_.reset();
  note_tracker_.reset();

  // Tracked sync metadata has just been thrown away. Depending on the current
  // selected behavior, notes themselves may need clearing too.
  TriggerWipeModelUponSyncDisabledBehavior();
}

void NoteDataTypeProcessor::TriggerWipeModelUponSyncDisabledBehavior() {
  switch (wipe_model_upon_sync_disabled_behavior_) {
    case syncer::WipeModelUponSyncDisabledBehavior::kNever:
      // Nothing to do.
      break;
    case syncer::WipeModelUponSyncDisabledBehavior::kOnceIfTrackingMetadata:
      // Do it this time, but switch to kNever so it doesn't trigger next
      // time.
      wipe_model_upon_sync_disabled_behavior_ =
          syncer::WipeModelUponSyncDisabledBehavior::kNever;
      [[fallthrough]];
    case syncer::WipeModelUponSyncDisabledBehavior::kAlways:
      notes_model_->RemoveAllSyncableNodes();
      break;
  }
}

}  // namespace sync_notes
