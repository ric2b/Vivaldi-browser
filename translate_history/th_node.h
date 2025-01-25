// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef TRANSLATE_HISTORY_TH_NODE_H_
#define TRANSLATE_HISTORY_TH_NODE_H_

#include "base/time/time.h"

#include <string>
#include <vector>

namespace translate_history {

class TH_Node {
 public:
  struct TextEntry {
    std::string code;
    std::string text;
  };

  TH_Node(const std::string& id);
  ~TH_Node();
  TH_Node(const TH_Node&) = delete;
  TH_Node& operator=(const TH_Node&) = delete;

  std::string id() const { return id_; }
  const base::Time& date_added() const { return date_added_; }
  void set_date_added(const base::Time& date) { date_added_ = date; }
  const TextEntry& src() const { return src_; }
  const TextEntry& translated() const { return translated_; }
  // Overloaded for effcient setup.
  TextEntry& src() { return src_; }
  TextEntry& translated() { return translated_; }

 private:
  std::string id_;
  TextEntry src_;
  TextEntry translated_;
  base::Time date_added_;
};

// typedef std::vector<std::unique_ptr<TH_Node>> NodeList;
typedef std::vector<TH_Node*> NodeList;

}  // namespace translate_history

#endif  // TRANSLATE_HISTORY_TH_NODE_H_
