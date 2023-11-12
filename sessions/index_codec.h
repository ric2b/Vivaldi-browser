// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef SESSIONS_INDEX_CODEC_H_
#define SESSIONS_INDEX_CODEC_H_

#include "base/files/file_path.h"
#include <map>
#include <set>
#include <string>

namespace base {
class Value;
}  // namespace base

namespace sessions {

class Index_Model;
class Index_Node;

// Decodes JSON values at a Menu_Model and encodes a Menu_Model into JSON.
class IndexCodec {
 public:
  typedef std::map<std::string, int> StringToIntMap;

  IndexCodec();
  ~IndexCodec();
  IndexCodec(const IndexCodec&) = delete;
  IndexCodec& operator=(const IndexCodec&) = delete;

  // Looks up and returns the file version.
  bool GetVersion(std::string* version, const base::Value& value);

  // Decodes straight from the files in the directory. This can be used when
  // there is no JSON spec present (typically the first time an existing install
  // loads session code).
  bool Decode(Index_Node* items,
              const base::FilePath& directory,
              const base::FilePath::StringPieceType& index_name);

  // Decodes JSON into a Index_Model object. Returns true on success,
  // false otherwise.
  // Offical builds will fail immediately on error, whiole internal will
  // continue as long as possible to simplify debugging. Errors are logged.
  bool Decode(Index_Node* items, Index_Node* backup, Index_Node* persistent,
              const base::Value& value);

  // Reads fields from 'value' and assigns to 'node'.
  void SetNodeFields(Index_Node* node,  Index_Node* parent,
                     const base::Value& value);

  // Encodes the model to a corresponding JSON value tree.
  base::Value Encode(Index_Model* model);

  // Fetches information of tabs, windows and optionally workspaces from the
  // session file itself.
  static void GetSessionContentInfo(base::FilePath name, int* num_windows,
    int* num_tabs, StringToIntMap* workspaces = nullptr);

 private:
  typedef std::map<std::string, bool> StringToBoolMap;
  bool DecodeNode(Index_Node* parent, const base::Value& value);
  base::Value EncodeNode(Index_Node* node);

  StringToBoolMap guids_;
};

}  // namespace sessions

#endif  // SESSIONS_INDEX_CODEC_H_
