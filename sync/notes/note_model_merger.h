// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTES_NOTE_MODEL_MERGER_H_
#define SYNC_NOTES_NOTE_MODEL_MERGER_H_

#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine/commit_and_get_updates_types.h"

namespace vivaldi {
class NoteNode;
}  // namespace vivaldi

namespace sync_notes {

class NoteModelView;
class SyncedNoteTracker;

// Responsible for merging local and remote note models when note sync
// is enabled for the first time by the user (i.e. no sync metadata exists
// locally), so we need a best-effort merge based on similarity. It is used by
// the NoteDataTypeProcessor().
class NoteModelMerger {
 public:
  // |note_model| and |note_tracker| must not be
  // null and must outlive this object.
  NoteModelMerger(syncer::UpdateResponseDataList updates,
                  NoteModelView* notes_model,
                  SyncedNoteTracker* note_tracker);
  NoteModelMerger(const NoteModelMerger&) = delete;
  NoteModelMerger& operator=(const NoteModelMerger&) = delete;

  ~NoteModelMerger();

  // Merges the remote note model represented as the updates received from
  // the sync server and local note model |notes_model|, and updates the
  // model and |note_tracker| (all of which are injected in the constructor)
  // accordingly. On return, there will be a 1:1 mapping between notes nodes
  // and metadata entities in the injected tracker.
  void Merge();

 private:
  // Internal representation of a remote tree, composed of nodes.
  class RemoteTreeNode final {
   private:
    using UpdatesPerParentUuid =
        std::unordered_map<base::Uuid,
                           std::list<syncer::UpdateResponseData>,
                           base::UuidHash>;

   public:
    // Constructs a tree given |update| as root and recursively all descendants
    // by traversing |*updates_per_parent_uuid|. |update| and
    // |updates_per_parent_uuid| must not be null. All updates
    // |*updates_per_parent_uuid| must represent valid updates. Updates
    // corresponding from descendant nodes are moved away from
    // |*updates_per_parent_uuid|. |max_depth| is the max tree depth to sync
    // after which content is silently ignored.
    static RemoteTreeNode BuildTree(
        syncer::UpdateResponseData update,
        size_t max_depth,
        UpdatesPerParentUuid* updates_per_parent_uuid);

    ~RemoteTreeNode();

    // Allow moves, useful during construction.
    RemoteTreeNode(RemoteTreeNode&&);
    RemoteTreeNode& operator=(RemoteTreeNode&&);

    const syncer::EntityData& entity() const { return update_.entity; }
    int64_t response_version() const { return update_.response_version; }

    // Direct children nodes, sorted by ascending unique position. These are
    // guaranteed to be valid updates (e.g. IsValidNotesSpecifics()).
    const std::vector<RemoteTreeNode>& children() const { return children_; }

    // Recursively emplaces all UUIDs (this node and descendants) into
    // |*uuid_to_remote_node_map|, which must not be null.
    void EmplaceSelfAndDescendantsByUuid(
        std::unordered_map<base::Uuid, const RemoteTreeNode*, base::UuidHash>*
            uuid_to_remote_node_map) const;

   private:
    static bool UniquePositionLessThan(const RemoteTreeNode& lhs,
                                       const RemoteTreeNode& rhs);

    RemoteTreeNode();

    syncer::UpdateResponseData update_;
    // Redundant, parsed instance of the unique position in specifics, used
    // to sort siblings by their position information.
    syncer::UniquePosition unique_position_;
    std::vector<RemoteTreeNode> children_;
  };

  // A forest composed of multiple trees where the root of each tree represents
  // a permanent node, keyed by server-defined unique tag of the root.
  using RemoteForest = std::unordered_map<std::string, RemoteTreeNode>;

  // Represents a pair of notes, one local and one remote, that have been
  // matched by UUID. They are guaranteed to have the same type and URL (if
  // applicable).
  struct GuidMatch {
    raw_ptr<const vivaldi::NoteNode> local_node = nullptr;
    raw_ptr<const RemoteTreeNode> remote_node = nullptr;
  };

  // Constructs the remote note tree to be merged. Each entry in the
  // returned map is a permanent node, identified (keyed) by the server-defined
  // tag. All invalid updates are filtered out, including invalid note
  // specifics as well as tombstones, in the unlikely event that the server
  // sends tombstones as part of the initial download.
  // |tracker_for_recording_ignored_updates| must not be null and is exclusively
  // used to record which updates where ignored because their parent couldn't be
  // determined.
  static RemoteForest BuildRemoteForest(
      syncer::UpdateResponseDataList updates,
      SyncedNoteTracker* tracker_for_recording_ignored_updates);

  // Computes note pairs that should be matched by UUID. Local note
  // UUIDs may be regenerated for the case where they collide with a remote UUID
  // that is not compatible (e.g. folder vs non-folder).
  static std::unordered_map<base::Uuid, GuidMatch, base::UuidHash>
  FindGuidMatchesOrReassignLocal(const RemoteForest& remote_forest,
                                 NoteModelView* note_model);

  // Merges a local and a remote subtrees. The input nodes are two equivalent
  // local and remote nodes. This method tries to recursively match their
  // children. It updates the |note_tracker_| accordingly.
  void MergeSubtree(const vivaldi::NoteNode* local_node,
                    const RemoteTreeNode& remote_node);

  // Makes a second pass on previously-merged subtree to detect if any of the
  // remote updates are lacking a client tag hash. If so, it migrates the entity
  // by issuing a deletion and a creation, using a new random GUID.
  void MigrateNotesInSubtreeWithoutClientTagHash(
      const RemoteTreeNode& remote_node);

  // Updates |local_node| to hold same UUID and semantics as its |remote_node|
  // match. The input nodes are two equivalent local and remote notes that
  // are about to be merged. The output node is the potentially replaced
  // |local_node|. |local_node| must not be a NotePermanentNode.
  const vivaldi::NoteNode* UpdateNoteNodeFromSpecificsIncludingUuid(
      const vivaldi::NoteNode* local_node,
      const RemoteTreeNode& remote_node);

  // Creates a local note node for a |remote_node|. The local node is
  // created under |local_parent| at position |index|. If the remote node has
  // children, this method recursively creates them as well. It updates the
  // |note_tracker_| accordingly.
  void ProcessRemoteCreation(const RemoteTreeNode& remote_node,
                             const vivaldi::NoteNode* local_parent,
                             size_t index);

  // Creates a server counter-part for the local node at position |index|
  // under |parent|. If the local node has children, corresponding server nodes
  // are created recursively. It updates the |note_tracker_| accordingly and
  // new nodes are marked to be committed.
  void ProcessLocalCreation(const vivaldi::NoteNode* parent, size_t index);

  // Looks for a local node under |local_parent| that matches |remote_node|,
  // starting at index |local_child_start_index|. First attempts to find a match
  // by UUID and otherwise attempts to find one by semantics. If no match is
  // found, a nullptr is returned.
  const vivaldi::NoteNode* FindMatchingLocalNode(
      const RemoteTreeNode& remote_node,
      const vivaldi::NoteNode* local_parent,
      size_t local_child_start_index) const;

  // If |local_node| has a remote counterpart of the same UUID, returns the
  // corresponding remote node, otherwise returns a nullptr. |local_node| must
  // not be null.
  const RemoteTreeNode* FindMatchingRemoteNodeByUuid(
      const vivaldi::NoteNode* local_node) const;

  // If |remote_node| has a local counterpart of the same UUID, returns the
  // corresponding local node, otherwise returns a nullptr.
  const vivaldi::NoteNode* FindMatchingLocalNodeByUuid(
      const RemoteTreeNode& remote_node) const;

  // Tries to find a child local node under |local_parent| that matches
  // |remote_node| semantically and returns the index of that node, as long as
  // this local child cannot be matched by UUID to a different node. Matching is
  // decided using NodeSemanticsMatch(). It searches in the children list
  // starting from position |search_starting_child_index|. In case of no match
  // is found, it returns |kInvalidIndex|.
  size_t FindMatchingChildBySemanticsStartingAt(
      const RemoteTreeNode& remote_node,
      const vivaldi::NoteNode* local_parent,
      size_t starting_child_index) const;

  // Used to generate a unique position for the current locally created
  // note.
  syncer::UniquePosition GenerateUniquePositionForLocalCreation(
      const vivaldi::NoteNode* parent,
      size_t index,
      const syncer::UniquePosition::Suffix& suffix) const;

  const raw_ptr<NoteModelView> notes_model_;
  const raw_ptr<SyncedNoteTracker> note_tracker_;
  // Preprocessed remote nodes in the form a forest where each tree's root is a
  // permanent node. Computed upon construction via BuildRemoteForest().
  const RemoteForest remote_forest_;
  std::unordered_map<base::Uuid, GuidMatch, base::UuidHash> uuid_to_match_map_;
};

}  // namespace sync_notes

#endif  // SYNC_NOTES_NOTE_MODEL_MERGER_H_
