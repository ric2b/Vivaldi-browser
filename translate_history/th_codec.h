// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef TRANSLATE_HISTORY_TH_CODEC_H_
#define TRANSLATE_HISTORY_TH_CODEC_H_

#include <vector>

#include "translate_history/th_node.h"

namespace base {
class Value;
}  // namespace base

namespace translate_history {

// Decodes and encodes JSON values to and from a NodeList.
class TH_Codec {
 public:
  TH_Codec();
  ~TH_Codec();
  TH_Codec(const TH_Codec&) = delete;
  TH_Codec& operator=(const TH_Codec&) = delete;

  // Decodes JSON into a NodeList object. Returns true on success,
  // false otherwise.
  bool Decode(NodeList& list, const base::Value& value);

  // Encodes the node list to a corresponding JSON value list. The list is
  // returned.
  base::Value Encode(NodeList& list);

 private:
  bool DecodeNode(NodeList& list, const base::Value& value);
  bool DecodeTextEntry(TH_Node::TextEntry& entry, const base::Value& value);

  base::Value EncodeNode(const TH_Node& node);
};

}  // namespace translate_history

#endif  // TRANSLATE_HISTORY_TH_CODEC_H_
