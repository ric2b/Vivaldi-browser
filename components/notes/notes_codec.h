// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTES_NOTES_CODEC_H_
#define COMPONENTS_NOTES_NOTES_CODEC_H_

#include <set>
#include <string>
#include <vector>

#include "base/uuid.h"
#include "base/hash/md5.h"
#include "components/notes/note_node.h"
#include "components/notes/notes_model.h"

namespace base {
class Value;
}  // namespace base

namespace vivaldi {

// NotesCodec is responsible for encoding and decoding the NotesModel
// into JSON values. The encoded values are written to disk via the
// NotesStorage.
class NotesCodec {
 public:
  // Creates an instance of the codec. During decoding, if the IDs in the file
  // are not unique, we will reassign IDs to make them unique. There are no
  // guarantees on how the IDs are reassigned or about doing minimal
  // reassignments to achieve uniqueness.
  NotesCodec();
  ~NotesCodec();
  NotesCodec(const NotesCodec&) = delete;
  NotesCodec& operator=(const NotesCodec&) = delete;

  // Encodes the model to a JSON value. This is invoked to encode the contents
  // of the notes model and is currently a convenience to invoking Encode that
  // takes the notes node and other folder node.
  base::Value Encode(NotesModel* model, const std::string& sync_metadata_str);

  // Encodes the notes folder returning the JSON value.
  base::Value Encode(const NoteNode* notes_node,
                     const NoteNode* other_notes_node,
                     const NoteNode* trash_notes_node,
                     const std::string& sync_metadata_str);

  // Decodes the previously encoded value to the specified nodes as well as
  // setting |max_node_id| to the greatest node id. Returns true on success,
  // false otherwise. If there is an error (such as unexpected version) all
  // children are removed from the notes and other folder nodes. On exit
  // |max_node_id| is set to the max id of the nodes.
  bool Decode(NoteNode* notes_node,
              NoteNode* other_notes_node,
              NoteNode* trash_notes_node,
              int64_t* max_node_id,
              const base::Value& value,
              std::string* sync_metadata_str);

  // Updates the check-sum with the given string.
  void UpdateChecksum(const std::string& str);
  void UpdateChecksum(const std::u16string& str);

  // Returns the checksum computed during last encoding/decoding call.
  const std::string& computed_checksum() const { return computed_checksum_; }

  // Returns the checksum that's stored in the file. After a call to Encode,
  // the computed and stored checksums are the same since the computed checksum
  // is stored to the file. After a call to decode, the computed checksum can
  // differ from the stored checksum if the file contents were changed by the
  // user.
  const std::string& stored_checksum() const { return stored_checksum_; }

  // Returns whether the IDs were reassigned during decoding. Always returns
  // false after encoding.
  bool ids_reassigned() const { return ids_reassigned_; }

  // Returns whether the UUIDs were reassigned during decoding. Always returns
  // false after encoding.
  bool uuids_reassigned() const { return uuids_reassigned_; }

  // Returns whether attachments using the old, deprecated format were found
  // during decoding.
  bool has_deprecated_attachments() const {
    return has_deprecated_attachments_;
  }

  // Names of the various keys written to the Value.
  static const char kVersionKey[];
  static const char kChecksumKey[];
  static const char kIdKey[];
  static const char kTypeKey[];
  static const char kSubjectKey[];
  static const char kDateAddedKey[];
  static const char kDateModifiedKey[];
  static const char kURLKey[];
  static const char kChildrenKey[];
  static const char kContentKey[];
  static const char kGuidKey[];
  static const char kAttachmentsKey[];
  static const char kSyncMetadata[];
  static const char kTypeNote[];
  static const char kTypeFolder[];
  static const char kTypeSeparator[];
  static const char kTypeAttachment[];
  static const char kTypeOther[];
  static const char kTypeTrash[];

 private:
  // Encodes node and all its children into a Value object and returns it.
  // The caller takes ownership of the returned object.
  base::Value EncodeNode(const NoteNode* node,
                         const std::vector<const NoteNode*>* extra_nodes);

  // Helper to perform decoding.
  bool DecodeHelper(NoteNode* notes_node,
                    NoteNode* other_notes_node,
                    NoteNode* trash_notes_node,
                    const base::Value& value,
                    std::string* sync_metadata_str);
  void ExtractSpecialNode(NoteNode::Type type,
                          NoteNode* source,
                          NoteNode* target);

  // Reassigns Notes IDs for all nodes.
  void ReassignIDs(NoteNode* notes_node,
                   NoteNode* other_node,
                   NoteNode* trash_node);

  // Helper to recursively reassign IDs.
  void ReassignIDsHelper(NoteNode* node);

  // Decodes the supplied node from the supplied value. Child nodes are
  // created appropriately by way of DecodeChildren. If node is NULL a new
  // node is created and added to parent (parent must then be non-NULL),
  // otherwise node is used.
  bool DecodeNode(const base::Value& value,
                  NoteNode* parent,
                  NoteNode* node,
                  NoteNode* child_other_node,
                  NoteNode* child_trash_node);

  // Initializes/Finalizes the checksum.
  void InitializeChecksum();
  void FinalizeChecksum();

  // Whether or not IDs were reassigned by the codec.
  bool ids_reassigned_;

  // Whether or not UUIDs were reassigned by the codec.
  bool uuids_reassigned_;

  // Whether or not IDs are valid. This is initially true, but set to false
  // if an id is missing or not unique.
  bool ids_valid_;

  // Whether the loaded notes have attachments using the old, deprecated format.
  bool has_deprecated_attachments_ = false;

  // Contains the id of each of the nodes found in the file. Used to determine
  // if we have duplicates.
  std::set<int64_t> ids_;

  // Contains the UUID of each of the nodes found in the file. Used to determine
  // if we have duplicates.
  std::set<base::Uuid> uuids_;

  // MD5 context used to compute MD5 hash of all notes data.
  base::MD5Context md5_context_;

  // Checksums.
  std::string computed_checksum_;
  std::string stored_checksum_;

  // Maximum ID assigned when decoding data.
  int64_t maximum_id_;
};

}  // namespace vivaldi

#endif  // COMPONENTS_NOTES_NOTES_CODEC_H_
