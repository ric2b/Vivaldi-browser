// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/glue/notes_model_associator.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sync/base/cryptographer.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/engine/engine_util.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/sync_merge_result.h"
#include "components/sync/syncable/entry.h"
#include "components/sync/syncable/read_node.h"
#include "components/sync/syncable/read_transaction.h"
#include "components/sync/syncable/syncable_write_transaction.h"
#include "components/sync/syncable/write_node.h"
#include "components/sync/syncable/write_transaction.h"
#include "content/public/browser/browser_thread.h"
#include "notes/notes_model.h"
#include "notes/notesnode.h"
#include "sync/glue/notes_change_processor.h"
#include "sync/internal_api/notes_delete_journal.h"

using content::BrowserThread;

namespace vivaldi {

// The sync protocol identifies top-level entities by means of well-known tags,
// which should not be confused with titles.  Each tag corresponds to a
// singleton instance of a particular top-level node in a user's share; the
// tags are consistent across users. The tags allow us to locate the specific
// folders whose contents we care about synchronizing, without having to do a
// lookup by name or path.  The tags should not be made user-visible.
//
// It is the responsibility of something upstream (at time of writing,
// the sync server) to create these tagged nodes when initializing sync
// for the first time for a user.  Thus, once the backend finishes
// initializing, the ProfileSyncService can rely on the presence of tagged
// nodes.

const char kNotesRootTag[] = "main_notes";
const char kNotesOtherTag[] = "other_notes";
const char kNotesTrashTag[] = "trash_notes";

// Maximum number of bytes to allow in a title (must match sync's internal
// limits; see write_node.cc).
const int kTitleLimitBytes = 255;

// Provides the following abstraction: given a parent notes node, find best
// matching child node for many sync nodes.
class NotesNodeFinder {
 public:
  // Creates an instance with the given parent notes node.
  explicit NotesNodeFinder(const Notes_Node* parent_node);

  // Finds the notes node that matches the given url, title and folder
  // attribute. Returns the matching node if one exists; NULL otherwise. If a
  // matching node is found, it's removed for further matches.
  const Notes_Node* FindNotesNode(const GURL& url,
                                  const std::string& title,
                                  const std::string& content,
                                  int special_node_type,
                                  bool is_folder,
                                  int64_t preferred_id);

  // Returns true if |notes_node| matches the specified |url|,
  // |title|, and |is_folder| flags.
  static bool NodeMatches(const Notes_Node* notes_node,
                          const GURL& url,
                          const std::string& title,
                          const std::string& content,
                          bool is_folder);

 private:
  // Maps nodes node titles to instances, duplicates allowed.
  // Titles are converted to the sync internal format before
  // being used as keys for the map.
  typedef base::hash_multimap<std::string, const Notes_Node*> NotesNodeMap;
  typedef std::pair<NotesNodeMap::iterator, NotesNodeMap::iterator>
      NotesNodeRange;

  // Converts and truncates note titles in the form sync does internally
  // to avoid mismatches due to sync munging titles.
  static void ConvertTitleToSyncInternalFormat(const std::string& input,
                                               std::string* output);

  const Notes_Node* parent_node_;
  NotesNodeMap child_nodes_;

  DISALLOW_COPY_AND_ASSIGN(NotesNodeFinder);
};

class ScopedAssociationUpdater {
 public:
  explicit ScopedAssociationUpdater(Notes_Model* model) {
    model_ = model;
    model->BeginExtensiveChanges();
  }

  ~ScopedAssociationUpdater() { model_->EndExtensiveChanges(); }

 private:
  Notes_Model* model_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAssociationUpdater);
};

NotesNodeFinder::NotesNodeFinder(const Notes_Node* parent_node)
    : parent_node_(parent_node) {
  for (int i = 0; i < parent_node_->child_count(); ++i) {
    const Notes_Node* child_node = parent_node_->GetChild(i);

    std::string title = base::UTF16ToUTF8(child_node->GetTitle());

    child_nodes_.insert(std::make_pair(title, child_node));
  }
}

const Notes_Node* NotesNodeFinder::FindNotesNode(const GURL& url,
                                                 const std::string& title,
                                                 const std::string& content,
                                                 int special_node_type,
                                                 bool is_folder,
                                                 int64_t preferred_id) {
  const Notes_Node* match = nullptr;

  // First lookup a range of notes with the same title.
  NotesNodeRange range = child_nodes_.equal_range(title);
  NotesNodeMap::iterator match_iter = range.second;
  for (NotesNodeMap::iterator iter = range.first; iter != range.second;
       ++iter) {
    // Then within the range match the node by the folder bit
    // and the url.
    const Notes_Node* node = iter->second;
    if (is_folder != node->is_folder())
      continue;
    if (url != node->GetURL())
      continue;
    if (node->is_trash() &&
        special_node_type != sync_pb::NotesSpecifics::TRASH_NODE)
      continue;
    if (node->is_separator() &&
        special_node_type != sync_pb::NotesSpecifics::SEPARATOR)
      continue;

    if (node->id() == preferred_id || preferred_id == 0) {
      // Preferred match - use this node.
      match = node;
      match_iter = iter;
      break;
    } else if (match == nullptr) {
      if (base::UTF16ToUTF8(node->GetContent()) == content) {
        // First match - continue iterating.
        match = node;
        match_iter = iter;
      }
    }
  }

  if (match_iter != range.second) {
    // Remove the matched node so we don't match with it again.
    child_nodes_.erase(match_iter);
  }

  return match;
}

/* static */
bool NotesNodeFinder::NodeMatches(const Notes_Node* notes_node,
                                  const GURL& url,
                                  const std::string& title,
                                  const std::string& content,
                                  bool is_folder) {
  if (url != notes_node->GetURL() || is_folder != notes_node->is_folder() ||
      content != base::UTF16ToUTF8(notes_node->GetContent())) {
    return false;
  }

  std::string note_title = base::UTF16ToUTF8(notes_node->GetTitle());

  // We used to skip converting the title upon saving encrypted notes,
  // this is a fix to support that.
  if (title.empty() && note_title.empty())

  // The title passed to this method comes from a sync directory entry.
  // The following line is needed to make the native note title comparable.
  ConvertTitleToSyncInternalFormat(note_title, &note_title);
  return title == note_title;
}

/* static */
void NotesNodeFinder::ConvertTitleToSyncInternalFormat(
    const std::string& input,
    std::string* output) {
  syncer::SyncAPINameToServerName(input, output);
  base::TruncateUTF8ToByteSize(*output, kTitleLimitBytes, output);
}

NotesModelAssociator::Context::Context(
    syncer::SyncMergeResult* local_merge_result,
    syncer::SyncMergeResult* syncer_merge_result)
    : local_merge_result_(local_merge_result),
      syncer_merge_result_(syncer_merge_result),
      duplicate_count_(0),
      native_model_sync_state_(UNSET) {}

NotesModelAssociator::Context::~Context() {}

void NotesModelAssociator::Context::PushNode(int64_t sync_id) {
  dfs_stack_.push(sync_id);
}

bool NotesModelAssociator::Context::PopNode(int64_t* sync_id) {
  if (dfs_stack_.empty()) {
    *sync_id = 0;
    return false;
  }
  *sync_id = dfs_stack_.top();
  dfs_stack_.pop();
  return true;
}

void NotesModelAssociator::Context::SetPreAssociationVersions(
    int64_t native_version,
    int64_t sync_version) {
  local_merge_result_->set_pre_association_version(native_version);
  syncer_merge_result_->set_pre_association_version(sync_version);
}

void NotesModelAssociator::Context::SetNumItemsBeforeAssociation(int local_num,
                                                                 int sync_num) {
  local_merge_result_->set_num_items_before_association(local_num);
  syncer_merge_result_->set_num_items_before_association(sync_num);
}

void NotesModelAssociator::Context::SetNumItemsAfterAssociation(int local_num,
                                                                int sync_num) {
  local_merge_result_->set_num_items_after_association(local_num);
  syncer_merge_result_->set_num_items_after_association(sync_num);
}

void NotesModelAssociator::Context::IncrementLocalItemsDeleted() {
  local_merge_result_->set_num_items_deleted(
      local_merge_result_->num_items_deleted() + 1);
}

void NotesModelAssociator::Context::IncrementLocalItemsAdded() {
  local_merge_result_->set_num_items_added(
      local_merge_result_->num_items_added() + 1);
}

void NotesModelAssociator::Context::IncrementLocalItemsModified() {
  local_merge_result_->set_num_items_modified(
      local_merge_result_->num_items_modified() + 1);
}

void NotesModelAssociator::Context::IncrementSyncItemsAdded() {
  syncer_merge_result_->set_num_items_added(
      syncer_merge_result_->num_items_added() + 1);
}

void NotesModelAssociator::Context::IncrementSyncItemsDeleted(int count) {
  syncer_merge_result_->set_num_items_deleted(
      syncer_merge_result_->num_items_deleted() + count);
}

void NotesModelAssociator::Context::UpdateDuplicateCount(
    const base::string16& title,
    const base::string16& content,
    const GURL& url) {
  // base::Hash is defined for 8-byte strings only so have to
  // cast the title data to char* and double the length in order to
  // compute its hash.
  size_t notes_hash = base::Hash(reinterpret_cast<const char*>(title.data()),
                                 title.length() * 2) ^
                      base::Hash(url.spec()) ^
                      base::Hash(reinterpret_cast<const char*>(content.data()),
                                 content.length() * 2);

  if (!hashes_.insert(notes_hash).second) {
    // This hash code already exists in the set.
    ++duplicate_count_;
  }
}

void NotesModelAssociator::Context::AddNotesRoot(const Notes_Node* root) {
  notes_roots_.push_back(root);
}

int64_t NotesModelAssociator::Context::GetSyncPreAssociationVersion() const {
  return syncer_merge_result_->pre_association_version();
}

void NotesModelAssociator::Context::MarkForVersionUpdate(
    const Notes_Node* node) {
  notes_for_version_update_.push_back(node);
}

NotesModelAssociator::NotesModelAssociator(
    Notes_Model* notes_model,
    syncer::SyncClient* sync_client,
    syncer::UserShare* user_share,
    std::unique_ptr<syncer::DataTypeErrorHandler> unrecoverable_error_handler)
    : notes_model_(notes_model),
      sync_client_(sync_client),
      user_share_(user_share),
      unrecoverable_error_handler_(std::move(unrecoverable_error_handler)),
      weak_factory_(this) {
  DCHECK(notes_model_);
  DCHECK(user_share_);
  DCHECK(unrecoverable_error_handler_);
}

NotesModelAssociator::~NotesModelAssociator() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

syncer::SyncError NotesModelAssociator::DisassociateModels() {
  id_map_.clear();
  id_map_inverse_.clear();
  dirty_associations_sync_ids_.clear();
  return syncer::SyncError();
}

int64_t NotesModelAssociator::GetSyncIdFromChromeId(const int64_t& node_id) {
  NotesIdToSyncIdMap::const_iterator iter = id_map_.find(node_id);
  return iter == id_map_.end() ? syncer::kInvalidId : iter->second;
}

const Notes_Node* NotesModelAssociator::GetChromeNodeFromSyncId(
    int64_t sync_id) {
  SyncIdToNotesNodeMap::const_iterator iter = id_map_inverse_.find(sync_id);
  return iter == id_map_inverse_.end() ? NULL : iter->second;
}

bool NotesModelAssociator::InitSyncNodeFromChromeId(
    const int64_t& node_id,
    syncer::BaseNode* sync_node) {
  DCHECK(sync_node);
  int64_t sync_id = GetSyncIdFromChromeId(node_id);
  if (sync_id == syncer::kInvalidId)
    return false;
  if (sync_node->InitByIdLookup(sync_id) != syncer::BaseNode::INIT_OK)
    return false;
  DCHECK(sync_node->GetId() == sync_id);
  return true;
}

void NotesModelAssociator::AddAssociation(const Notes_Node* node,
                                          int64_t sync_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  int64_t node_id = node->id();
  DCHECK_NE(sync_id, syncer::kInvalidId);
  DCHECK(id_map_.find(node_id) == id_map_.end());
  DCHECK(id_map_inverse_.find(sync_id) == id_map_inverse_.end());
  id_map_[node_id] = sync_id;
  id_map_inverse_[sync_id] = node;
}

void NotesModelAssociator::Associate(const Notes_Node* node,
                                     const syncer::BaseNode& sync_node) {
  AddAssociation(node, sync_node.GetId());

  // The same check exists in PersistAssociations. However it is better to
  // do the check earlier to avoid the cost of decrypting nodes again
  // in PersistAssociations.
  if (node->id() != sync_node.GetExternalId()) {
    dirty_associations_sync_ids_.insert(sync_node.GetId());
    PostPersistAssociationsTask();
  }
}

void NotesModelAssociator::Disassociate(int64_t sync_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SyncIdToNotesNodeMap::iterator iter = id_map_inverse_.find(sync_id);
  if (iter == id_map_inverse_.end())
    return;
  id_map_.erase(iter->second->id());
  id_map_inverse_.erase(iter);
  dirty_associations_sync_ids_.erase(sync_id);
}

bool NotesModelAssociator::SyncModelHasUserCreatedNodes(bool* has_nodes) {
  DCHECK(has_nodes);
  *has_nodes = false;

  syncer::ReadTransaction trans(FROM_HERE, user_share_);

  syncer::ReadNode main_note_node(&trans);
  if (main_note_node.InitByTagLookupForNotes(kNotesRootTag) !=
      syncer::BaseNode::INIT_OK) {
    return false;
  }

  syncer::ReadNode other_note_node(&trans);
  if (other_note_node.InitByTagLookupForNotes(kNotesOtherTag) !=
      syncer::BaseNode::INIT_OK) {
    return false;
  }
  syncer::ReadNode trash_note_node(&trans);
  if (trash_note_node.InitByTagLookupForNotes(kNotesTrashTag) !=
      syncer::BaseNode::INIT_OK) {
    return false;
  }

  // Sync model has user created nodes if any of the permanent nodes has
  // children.
  *has_nodes = main_note_node.HasChildren() || other_note_node.HasChildren() ||
               trash_note_node.HasChildren();

  return true;
}

bool NotesModelAssociator::AssociateTaggedPermanentNode(
    syncer::BaseTransaction* trans,
    const Notes_Node* permanent_node,
    const std::string& tag) {
  // Do nothing if |permanent_node| is already initialized and associated.
  int64_t sync_id = GetSyncIdFromChromeId(permanent_node->id());
  if (sync_id != syncer::kInvalidId)
    return true;

  syncer::ReadNode sync_node(trans);
  if (sync_node.InitByTagLookupForNotes(tag) != syncer::BaseNode::INIT_OK)
    return false;

  Associate(permanent_node, sync_node);
  return true;
}

syncer::SyncError NotesModelAssociator::AssociateModels(
    syncer::SyncMergeResult* local_merge_result,
    syncer::SyncMergeResult* syncer_merge_result) {
  Context context(local_merge_result, syncer_merge_result);

  syncer::SyncError error = CheckModelSyncState(&context);
  if (error.IsSet())
    return error;

  std::unique_ptr<ScopedAssociationUpdater> association_updater(
      new ScopedAssociationUpdater(notes_model_));
  DisassociateModels();

  error = BuildAssociations(&context);
  if (error.IsSet()) {
    // Clear version on notes model so that the conservative association
    // algorithm is used on the next association.
    notes_model_->SetNodeSyncTransactionVersion(
        notes_model_->root_node(),
        syncer::syncable::kInvalidTransactionVersion);
  }

  return error;
}

syncer::SyncError NotesModelAssociator::AssociatePermanentFolders(
    syncer::BaseTransaction* trans,
    Context* context) {
  // To prime our association, we associate the top-level nodes
  if (!AssociateTaggedPermanentNode(trans, notes_model_->main_node(),
                                    kNotesRootTag)) {
    return unrecoverable_error_handler_->CreateAndUploadError(
        FROM_HERE, "Notes Root node not found", model_type());
  }
  if (!AssociateTaggedPermanentNode(trans, notes_model_->other_node(),
                                    kNotesOtherTag)) {
    return unrecoverable_error_handler_->CreateAndUploadError(
        FROM_HERE, "Notes Other node not found", model_type());
  }
  if (!AssociateTaggedPermanentNode(trans, notes_model_->trash_node(),
                                    kNotesTrashTag)) {
    return unrecoverable_error_handler_->CreateAndUploadError(
        FROM_HERE, "Notes Trash node not found", model_type());
  }

  int64_t notes_root_sync_id =
      GetSyncIdFromChromeId(notes_model_->main_node()->id());
  DCHECK_NE(notes_root_sync_id, syncer::kInvalidId);
  context->AddNotesRoot(notes_model_->main_node());
  int64_t notes_other_sync_id =
      GetSyncIdFromChromeId(notes_model_->other_node()->id());
  DCHECK_NE(notes_other_sync_id, syncer::kInvalidId);
  context->AddNotesRoot(notes_model_->other_node());
  int64_t notes_trash_sync_id =
      GetSyncIdFromChromeId(notes_model_->trash_node()->id());
  DCHECK_NE(notes_trash_sync_id, syncer::kInvalidId);
  context->AddNotesRoot(notes_model_->trash_node());

  // WARNING: The order in which we push these should match their order in the
  // notes model (see NotesModel::DoneLoading(..)).
  context->PushNode(notes_root_sync_id);
  context->PushNode(notes_other_sync_id);
  context->PushNode(notes_trash_sync_id);

  return syncer::SyncError();
}

void NotesModelAssociator::SetNumItemsBeforeAssociation(
    syncer::BaseTransaction* trans,
    Context* context) {
  int syncer_num = 0;
  syncer::ReadNode bm_root(trans);
  if (bm_root.InitTypeRoot(syncer::NOTES) == syncer::BaseNode::INIT_OK) {
    syncer_num = bm_root.GetTotalNodeCount();
  }
  context->SetNumItemsBeforeAssociation(
      GetTotalNotesCountAndRecordDuplicates(notes_model_->root_node(), context),
      syncer_num);
}

int NotesModelAssociator::GetTotalNotesCountAndRecordDuplicates(
    const Notes_Node* node,
    Context* context) const {
  int count = 1;  // Start with one to include the node itself.

  if (!node->is_root()) {
    context->UpdateDuplicateCount(node->GetTitle(), node->GetContent(),
                                  node->GetURL());
  }

  for (int i = 0; i < node->child_count(); ++i) {
    count += GetTotalNotesCountAndRecordDuplicates(node->GetChild(i), context);
  }

  return count;
}

void NotesModelAssociator::SetNumItemsAfterAssociation(
    syncer::BaseTransaction* trans,
    Context* context) {
  int syncer_num = 0;
  syncer::ReadNode bm_root(trans);
  if (bm_root.InitTypeRoot(syncer::NOTES) == syncer::BaseNode::INIT_OK) {
    syncer_num = bm_root.GetTotalNodeCount();
  }
  context->SetNumItemsAfterAssociation(
      notes_model_->root_node()->GetTotalNodeCount(), syncer_num);
}

syncer::SyncError NotesModelAssociator::BuildAssociations(Context* context) {
  DCHECK(notes_model_->loaded());
  DCHECK_NE(context->native_model_sync_state(), AHEAD);

  int initial_duplicate_count = 0;
  int64_t new_version = syncer::syncable::kInvalidTransactionVersion;
  {
    syncer::WriteTransaction trans(FROM_HERE, user_share_, &new_version);

    syncer::SyncError error = AssociatePermanentFolders(&trans, context);
    if (error.IsSet())
      return error;

    SetNumItemsBeforeAssociation(&trans, context);
    initial_duplicate_count = context->duplicate_count();

    // Remove obsolete notes according to sync delete journal.
    // TODO(stanisc): crbug.com/456876: rewrite this to avoid a separate
    // traversal and instead perform deletes at the end of the loop below where
    // the unmatched notes nodes are created as sync nodes.
    ApplyDeletesFromSyncJournal(&trans, context);

    // Algorithm description:
    // Match up the roots and recursively do the following:
    // * For each sync node for the current sync parent node, find the best
    //   matching notes node under the corresponding notes parent node.
    //   If no matching node is found, create a new notes node in the same
    //   position as the corresponding sync node.
    //   If a matching node is found, update the properties of it from the
    //   corresponding sync node.
    // * When all children sync nodes are done, add the extra children notes
    //   nodes to the sync parent node.
    //
    // The best match algorithm uses folder title or notes title/url to
    // perform the primary match. If there are multiple match candidates it
    // selects the preferred one based on sync node external ID match to the
    // notes folder ID.
    int64_t sync_parent_id;
    while (context->PopNode(&sync_parent_id)) {
      syncer::ReadNode sync_parent(&trans);
      if (sync_parent.InitByIdLookup(sync_parent_id) !=
          syncer::BaseNode::INIT_OK) {
        return unrecoverable_error_handler_->CreateAndUploadError(
            FROM_HERE, "Failed to lookup node.", model_type());
      }
      // Only folder nodes are pushed on to the stack.
      DCHECK(sync_parent.GetIsFolder());

      const Notes_Node* parent_node = GetChromeNodeFromSyncId(sync_parent_id);
      if (!parent_node) {
        return unrecoverable_error_handler_->CreateAndUploadError(
            FROM_HERE, "Failed to find notes node for sync id.", model_type());
      }
      DCHECK(parent_node->is_folder());

      std::vector<int64_t> children;
      sync_parent.GetChildIds(&children);

      error = BuildAssociations(&trans, parent_node, children, context);
      if (error.IsSet())
        return error;
    }

    SetNumItemsAfterAssociation(&trans, context);
  }

  if (new_version == syncer::syncable::kInvalidTransactionVersion) {
    // If we get here it means that none of Sync nodes were modified by the
    // association process.
    // We need to set |new_version| to the pre-association Sync version;
    // otherwise NotesChangeProcessor::UpdateTransactionVersion call below
    // won't save it to the native model. That is necessary to ensure that the
    // native model doesn't get stuck at "unset" version and skips any further
    // version checks.
    new_version = context->GetSyncPreAssociationVersion();
  }

  NotesChangeProcessor::UpdateTransactionVersion(
      new_version, notes_model_, context->notes_for_version_update());

  UMA_HISTOGRAM_COUNTS("Sync.NotesDuplicationsAtAssociation",
                       context->duplicate_count());
  UMA_HISTOGRAM_COUNTS("Sync.NotesNewDuplicationsAtAssociation",
                       context->duplicate_count() - initial_duplicate_count);

  if (context->duplicate_count() > initial_duplicate_count) {
    UMA_HISTOGRAM_ENUMERATION("Sync.NotesModelSyncStateAtNewDuplication",
                              context->native_model_sync_state(),
                              NATIVE_MODEL_SYNC_STATE_COUNT);
  }

  return syncer::SyncError();
}

syncer::SyncError NotesModelAssociator::BuildAssociations(
    syncer::WriteTransaction* trans,
    const Notes_Node* parent_node,
    const std::vector<int64_t>& sync_ids,
    Context* context) {
  NotesNodeFinder node_finder(parent_node);

  int index = 0;
  for (std::vector<int64_t>::const_iterator it = sync_ids.begin();
       it != sync_ids.end(); ++it) {
    int64_t sync_child_id = *it;
    syncer::ReadNode sync_child_node(trans);
    if (sync_child_node.InitByIdLookup(sync_child_id) !=
        syncer::BaseNode::INIT_OK) {
      return unrecoverable_error_handler_->CreateAndUploadError(
          FROM_HERE, "Failed to lookup node.", model_type());
    }

    int64_t external_id = sync_child_node.GetExternalId();
    GURL url(sync_child_node.GetNotesSpecifics().url());
    const Notes_Node* child_node = node_finder.FindNotesNode(
        url, sync_child_node.GetTitle(),
        sync_child_node.GetNotesSpecifics().content(),
        sync_child_node.GetNotesSpecifics().special_node_type(),
        sync_child_node.GetIsFolder(), external_id);
    if (child_node) {
      // Skip local node update if the local model version matches and
      // the node is already associated and in the right position.
      bool is_in_sync = (context->native_model_sync_state() == IN_SYNC) &&
                        (child_node->id() == external_id) &&
                        (index < parent_node->child_count()) &&
                        (parent_node->GetChild(index) == child_node);
      if (!is_in_sync) {
        NotesChangeProcessor::UpdateNoteWithSyncData(
            sync_child_node, notes_model_, child_node, sync_client_);
        notes_model_->Move(child_node, parent_node, index);
        context->IncrementLocalItemsModified();
        context->MarkForVersionUpdate(child_node);
      }
    } else {
      syncer::SyncError error;
      child_node = CreateNotesNode(parent_node, index, &sync_child_node, url,
                                   context, &error);
      if (!child_node) {
        if (error.IsSet()) {
          return error;
        } else {
          // Skip this node and continue. Don't increment index in this case.
          continue;
        }
      }
      context->IncrementLocalItemsAdded();
      context->MarkForVersionUpdate(child_node);
    }

    Associate(child_node, sync_child_node);

    if (sync_child_node.GetIsFolder())
      context->PushNode(sync_child_id);
    ++index;
  }

  // At this point all the children nodes of the parent sync node have
  // corresponding children in the parent notes node and they are all in
  // the right positions: from 0 to index - 1.
  // So the children starting from index in the parent notes node are the
  // ones that are not present in the parent sync node. So create them.
  for (int i = index; i < parent_node->child_count(); ++i) {
    int64_t sync_child_id = NotesChangeProcessor::CreateSyncNode(
        parent_node, notes_model_, i, trans, this,
        unrecoverable_error_handler_.get());
    if (syncer::kInvalidId == sync_child_id) {
      return unrecoverable_error_handler_->CreateAndUploadError(
          FROM_HERE, "Failed to create sync node.", model_type());
    }

    context->IncrementSyncItemsAdded();
    const Notes_Node* child_node = parent_node->GetChild(i);
    context->MarkForVersionUpdate(child_node);
    if (child_node->is_folder())
      context->PushNode(sync_child_id);
  }

  return syncer::SyncError();
}

const Notes_Node* NotesModelAssociator::CreateNotesNode(
    const Notes_Node* parent_node,
    int notes_index,
    const syncer::BaseNode* sync_child_node,
    const GURL& url,
    Context* context,
    syncer::SyncError* error) {
  DCHECK_LE(notes_index, parent_node->child_count());

  const std::string& sync_title = sync_child_node->GetTitle();

  if (!sync_child_node->GetIsFolder() && sync_title.empty() &&
      !url.is_valid() &&
      sync_child_node->GetNotesSpecifics().content().empty()) {
    unrecoverable_error_handler_->CreateAndUploadError(
        FROM_HERE,
        "Cannot associate sync node " + sync_child_node->GetSyncId().value() +
            " with invalid url " + url.possibly_invalid_spec() + " and title " +
            sync_title,
        model_type());
    // Don't propagate the error to the model_type in this case.
    return nullptr;
  }

  base::string16 notes_title = base::UTF8ToUTF16(sync_title);
  const Notes_Node* child_node = NotesChangeProcessor::CreateNotesEntry(
      notes_title, url, sync_child_node, parent_node, notes_model_,
      sync_client_, notes_index);
  if (!child_node) {
    *error = unrecoverable_error_handler_->CreateAndUploadError(
        FROM_HERE,
        "Failed to create notes node with title " + sync_title + " and url " +
            url.possibly_invalid_spec(),
        model_type());
    return nullptr;
  }

  context->UpdateDuplicateCount(notes_title, child_node->GetContent(), url);
  return child_node;
}

int NotesModelAssociator::RemoveSyncNodeHierarchy(
    syncer::WriteTransaction* trans,
    int64_t sync_id) {
  syncer::WriteNode sync_node(trans);
  if (sync_node.InitByIdLookup(sync_id) != syncer::BaseNode::INIT_OK) {
    syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                            "Could not lookup notes node for ID deletion.",
                            syncer::NOTES);
    unrecoverable_error_handler_->OnUnrecoverableError(error);
    return 0;
  }

  return NotesChangeProcessor::RemoveSyncNodeHierarchy(trans, &sync_node, this);
}

struct FolderInfo {
  FolderInfo(const Notes_Node* f, const Notes_Node* p, int64_t id)
      : folder(f), parent(p), sync_id(id) {}
  const Notes_Node* folder;
  const Notes_Node* parent;
  int64_t sync_id;
};
typedef std::vector<FolderInfo> FolderInfoList;

void NotesModelAssociator::ApplyDeletesFromSyncJournal(
    syncer::BaseTransaction* trans,
    Context* context) {
  notessyncer::NotesDeleteJournalList bk_delete_journals;
  notessyncer::DeleteJournal::GetNotesDeleteJournals(trans,
                                                     &bk_delete_journals);
  if (bk_delete_journals.empty())
    return;

  size_t num_journals_unmatched = bk_delete_journals.size();

  // Make a set of all external IDs in the delete journal,
  // ignore entries with unset external IDs.
  std::set<int64_t> journaled_external_ids;
  for (size_t i = 0; i < num_journals_unmatched; i++) {
    if (bk_delete_journals[i].external_id != 0)
      journaled_external_ids.insert(bk_delete_journals[i].external_id);
  }

  // Check notes model from top to bottom.
  NotesStack dfs_stack;
  for (NotesList::const_iterator it = context->notes_roots().begin();
       it != context->notes_roots().end(); ++it) {
    dfs_stack.push(*it);
  }

  // Remember folders that match delete journals in first pass but don't delete
  // them in case there are notes left under them. After non-folder
  // notes are removed in first pass, recheck the folders in reverse order
  // to remove empty ones.
  FolderInfoList folders_matched;
  while (!dfs_stack.empty() && num_journals_unmatched > 0) {
    const Notes_Node* parent = dfs_stack.top();
    dfs_stack.pop();
    DCHECK(parent->is_folder());

    // Enumerate folder children in reverse order to make it easier to remove
    // notes matching entries in the delete journal.
    for (int child_index = parent->child_count() - 1;
         child_index >= 0 && num_journals_unmatched > 0; --child_index) {
      const Notes_Node* child = parent->GetChild(child_index);
      if (child->is_folder())
        dfs_stack.push(child);

      if (journaled_external_ids.find(child->id()) ==
          journaled_external_ids.end()) {
        // Skip notes node which id is not in the set of external IDs.
        continue;
      }

      // Iterate through the journal entries from back to front. Remove matched
      // journal by moving an unmatched entry at the tail to the matched
      // position so that we can read unmatched entries off the head in next
      // loop.
      for (int journal_index = num_journals_unmatched - 1; journal_index >= 0;
           --journal_index) {
        const notessyncer::NotesDeleteJournal& delete_entry =
            bk_delete_journals[journal_index];
        if (child->id() == delete_entry.external_id &&
            NotesNodeFinder::NodeMatches(
                child, GURL(delete_entry.specifics.notes().url()),
                delete_entry.specifics.notes().subject(),
                delete_entry.specifics.notes().content(),
                delete_entry.is_folder)) {
          if (child->is_folder()) {
            // Remember matched folder without removing and delete only empty
            // ones later.
            folders_matched.push_back(
                FolderInfo(child, parent, delete_entry.id));
          } else {
            notes_model_->Remove(child);
            context->IncrementLocalItemsDeleted();
          }
          // Move unmatched journal here and decrement counter.
          bk_delete_journals[journal_index] =
              bk_delete_journals[--num_journals_unmatched];
          break;
        }
      }
    }
  }

  // Ids of sync nodes not found in notes model, meaning the deletions are
  // persisted and correponding delete journals can be dropped.
  std::set<int64_t> journals_to_purge;

  // Remove empty folders from bottom to top.
  for (FolderInfoList::reverse_iterator it = folders_matched.rbegin();
       it != folders_matched.rend(); ++it) {
    if (it->folder->child_count() == 0) {
      notes_model_->Remove(it->folder);
      context->IncrementLocalItemsDeleted();
    } else {
      // Keep non-empty folder and remove its journal so that it won't match
      // again in the future.
      journals_to_purge.insert(it->sync_id);
    }
  }

  // Purge unmatched journals.
  for (size_t i = 0; i < num_journals_unmatched; ++i)
    journals_to_purge.insert(bk_delete_journals[i].id);
  notessyncer::DeleteJournal::PurgeDeleteJournals(trans, journals_to_purge);
}

void NotesModelAssociator::PostPersistAssociationsTask() {
  // No need to post a task if a task is already pending.
  if (weak_factory_.HasWeakPtrs())
    return;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&NotesModelAssociator::PersistAssociations,
                            weak_factory_.GetWeakPtr()));
}

void NotesModelAssociator::PersistAssociations() {
  // If there are no dirty associations we have nothing to do. We handle this
  // explicity instead of letting the for loop do it to avoid creating a write
  // transaction in this case.
  if (dirty_associations_sync_ids_.empty()) {
    DCHECK(id_map_.empty());
    DCHECK(id_map_inverse_.empty());
    return;
  }

  int64_t new_version = syncer::syncable::kInvalidTransactionVersion;
  std::vector<const Notes_Node*> bnodes;
  {
    syncer::WriteTransaction trans(FROM_HERE, user_share_, &new_version);
    DirtyAssociationsSyncIds::iterator iter;
    for (iter = dirty_associations_sync_ids_.begin();
         iter != dirty_associations_sync_ids_.end(); ++iter) {
      int64_t sync_id = *iter;
      syncer::WriteNode sync_node(&trans);
      if (sync_node.InitByIdLookup(sync_id) != syncer::BaseNode::INIT_OK) {
        syncer::SyncError error(
            FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
            "Could not lookup note node for ID persistence.", syncer::NOTES);
        unrecoverable_error_handler_->OnUnrecoverableError(error);
        return;
      }
      const Notes_Node* node = GetChromeNodeFromSyncId(sync_id);
      if (node && sync_node.GetExternalId() != node->id()) {
        sync_node.SetExternalId(node->id());
        bnodes.push_back(node);
      }
    }
    dirty_associations_sync_ids_.clear();
  }

  NotesChangeProcessor::UpdateTransactionVersion(new_version, notes_model_,
                                                 bnodes);
}

bool NotesModelAssociator::CryptoReadyIfNecessary() {
  // We only access the cryptographer while holding a transaction.
  syncer::ReadTransaction trans(FROM_HERE, user_share_);
  const syncer::ModelTypeSet encrypted_types = trans.GetEncryptedTypes();
  return !encrypted_types.Has(syncer::NOTES) ||
         trans.GetCryptographer()->is_ready();
}

syncer::SyncError NotesModelAssociator::CheckModelSyncState(
    Context* context) const {
  DCHECK_EQ(context->native_model_sync_state(), UNSET);
  int64_t native_version =
      notes_model_->root_node()->sync_transaction_version();
  syncer::ReadTransaction trans(FROM_HERE, user_share_);
  int64_t sync_version = trans.GetModelVersion(syncer::NOTES);
  context->SetPreAssociationVersions(native_version, sync_version);

  if (native_version != syncer::syncable::kInvalidTransactionVersion) {
    if (native_version == sync_version) {
      context->set_native_model_sync_state(IN_SYNC);
    } else {
      UMA_HISTOGRAM_ENUMERATION("Sync.LocalModelOutOfSync",
                                ModelTypeToHistogramInt(syncer::NOTES),
                                static_cast<int>(syncer::MODEL_TYPE_COUNT));

      // Clear version on notes model so that we only report error once.
      notes_model_->SetNodeSyncTransactionVersion(
          notes_model_->root_node(),
          syncer::syncable::kInvalidTransactionVersion);

      // If the native version is higher, there was a sync persistence failure,
      // and we need to delay association until after a GetUpdates.
      if (native_version > sync_version) {
        context->set_native_model_sync_state(AHEAD);
        std::string message =
            base::StringPrintf("Native version (%" PRId64
                               ") does not match sync version (%" PRId64 ")",
                               native_version, sync_version);
        return syncer::SyncError(FROM_HERE,
                                 syncer::SyncError::PERSISTENCE_ERROR, message,
                                 syncer::NOTES);
      } else {
        context->set_native_model_sync_state(BEHIND);
      }
    }
  }
  return syncer::SyncError();
}

}  // namespace vivaldi
