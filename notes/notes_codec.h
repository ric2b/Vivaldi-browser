// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NOTES_NOTES_CODEC_H_
#define NOTES_NOTES_CODEC_H_

#include <set>
#include <string>

#include "base/md5.h"
#include "base/strings/string16.h"
#include "notes/notes_model.h"
#include "notes/notesnode.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

namespace vivaldi {

// NotesCodec is responsible for encoding and decoding the Notes_Model
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

  // Encodes the model to a JSON value. It's up to the caller to delete the
  // returned object. This is invoked to encode the contents of the notes
  // model and is currently a convenience to invoking Encode that takes the
  // notes node and other folder node.
  std::unique_ptr<base::Value> Encode(Notes_Model* model);

  // Encodes the notes folder returning the JSON value. It's
  // up to the caller to delete the returned object.
  std::unique_ptr<base::Value> Encode(const Notes_Node* notes_node,
                      const Notes_Node* other_notes_node,
                      const Notes_Node* trash_notes_node,
                      int64_t sync_transaction_version);

  // Decodes the previously encoded value to the specified nodes as well as
  // setting |max_node_id| to the greatest node id. Returns true on success,
  // false otherwise. If there is an error (such as unexpected version) all
  // children are removed from the notes and other folder nodes. On exit
  // |max_node_id| is set to the max id of the nodes.
  bool Decode(Notes_Node* notes_node,
              Notes_Node* other_notes_node,
              Notes_Node* trash_notes_node,
              int64_t* max_node_id,
              const base::Value& value);

  // Updates the check-sum with the given string.
  void UpdateChecksum(const std::string& str);
  void UpdateChecksum(const base::string16& str);

  // Returns the checksum computed during last encoding/decoding call.
  const std::string& computed_checksum() const { return computed_checksum_; }

  // Returns the checksum that's stored in the file. After a call to Encode,
  // the computed and stored checksums are the same since the computed checksum
  // is stored to the file. After a call to decode, the computed checksum can
  // differ from the stored checksum if the file contents were changed by the
  // user.
  const std::string& stored_checksum() const { return stored_checksum_; }

  // Return the sync transaction version of the notes model root.
  int64_t model_sync_transaction_version() const {
    return model_sync_transaction_version_;
  }

  // Returns whether the IDs were reassigned during decoding. Always returns
  // false after encoding.
  bool ids_reassigned() const { return ids_reassigned_; }

  void register_id(int64_t id);
  size_t count_id(int64_t id) { return ids_.count(id); }
  bool ids_valid() const { return ids_valid_; }
  void set_ids_valid(bool val) { ids_valid_ = val; }

  // Names of the various keys written to the Value.
  static const char* kRootsKey;
  static const char* kVersionKey;
  static const char* kChecksumKey;
  static const char* kIdKey;
  static const char* kNameKey;
  static const char* kDateModifiedKey;
  static const char* kChildrenKey;
  static const char* kSyncTransactionVersion;

 private:
  // Encodes node and all its children into a Value object and returns it.
  // The caller takes ownership of the returned object.
  base::Value* EncodeNode(const Notes_Node* node);

  // Helper to perform decoding.
  bool DecodeHelper(Notes_Node* notes_node,
                    Notes_Node* other_notes_node,
                    Notes_Node* trash_notes_node,
                    const base::Value& value);
  void ExtractSpecialNode(Notes_Node::Type type,
                          Notes_Node* source,
                          Notes_Node* target);

  // Reassigns Notes IDs for all nodes.
  void ReassignIDs(Notes_Node* notes_node,
                   Notes_Node* other_node,
                   Notes_Node* trash_node);

  // Helper to recursively reassign IDs.
  void ReassignIDsHelper(Notes_Node* node);

  // Initializes/Finalizes the checksum.
  void InitializeChecksum();
  void FinalizeChecksum();

  // Whether or not IDs were reassigned by the codec.
  bool ids_reassigned_;

  // Whether or not IDs are valid. This is initially true, but set to false
  // if an id is missing or not unique.
  bool ids_valid_;

  // Contains the id of each of the nodes found in the file. Used to determine
  // if we have duplicates.
  std::set<int64_t> ids_;

  // MD5 context used to compute MD5 hash of all notes data.
  base::MD5Context md5_context_;

  // Checksums.
  std::string computed_checksum_;
  std::string stored_checksum_;

  // Maximum ID assigned when decoding data.
  int64_t maximum_id_;

  // Sync transaction version set on notes model root.
  int64_t model_sync_transaction_version_;

  DISALLOW_COPY_AND_ASSIGN(NotesCodec);
};

}  // namespace vivaldi

#endif  // NOTES_NOTES_CODEC_H_
