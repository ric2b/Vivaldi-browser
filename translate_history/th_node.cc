// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "translate_history/th_node.h"

namespace translate_history {

TH_Node::TH_Node(const std::string& id)
    : id_(id), date_added_(base::Time::Now()) {}

TH_Node::~TH_Node() = default;

}  // namespace translate_history