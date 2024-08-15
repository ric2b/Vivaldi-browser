// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "translate_history/th_codec.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>

#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "base/values.h"
#include "translate_history/th_node.h"

namespace translate_history {

TH_Codec::TH_Codec() = default;
TH_Codec::~TH_Codec() = default;

bool TH_Codec::Decode(NodeList& list, const base::Value& value) {
  if (!value.is_dict()) {
    LOG(ERROR) << "Translate History Codec: No dictionary";
    return false;
  }

  auto* format = value.GetDict().FindString("format");
  if (!format) {
    LOG(ERROR) << "Translate History Codec: No format specifier";
    return false;
  }

  const base::Value* children = value.GetDict().Find("children");
  if (!children || !children->is_list()) {
    LOG(ERROR) << "Translate History Codec: No children";
    return false;
  }

  for (const auto& node : children->GetList()) {
    bool parsing_ok = DecodeNode(list, node);
    if (!parsing_ok) {
      return false;
    }
  }

  return true;
}

bool TH_Codec::DecodeNode(NodeList& list, const base::Value& value) {
  auto* id = value.GetDict().FindString("id");
  bool guid_valid = id && !id->empty() &&
      base::Uuid::ParseCaseInsensitive(*id).is_valid();
  if (!guid_valid) {
    LOG(ERROR) << "Translate History Codec: Id missing or not valid";
    return false;
  }

  std::optional<base::Time> date_added =
      ValueToTime(value.GetDict().Find("date_added"));
  if (!date_added) {
    LOG(ERROR) << "Translate History Codec: Date added missing or not valid";
    return false;
  }

  const base::Value* src = value.GetDict().Find("src");
  const base::Value* translated = value.GetDict().Find("translated");
  if (!src || !translated) {
    LOG(ERROR) << "Translate History Codec: Content missing for " << *id;
    return false;
  }

  std::unique_ptr<TH_Node> node = std::make_unique<TH_Node>(*id);
  if (!DecodeTextEntry(node->src(), *src) ||
      !DecodeTextEntry(node->translated(), *translated)) {
    LOG(ERROR) << "Translate History Codec: Text entry missing for " << *id;
    return false;
  }

  node->set_date_added(*date_added);

  list.push_back(node.release());
  return true;
}

bool TH_Codec::DecodeTextEntry(TH_Node::TextEntry& entry,
                               const base::Value& value) {
  auto* code = value.GetDict().FindString("code");
  auto* text = value.GetDict().FindString("text");
  if (!code || !text) {
    return false;
  }
  entry.code = *code;
  entry.text = *text;
  return true;
}

base::Value TH_Codec::Encode(NodeList& nodes) {
  base::Value::List children;
  for (TH_Node* node : nodes) {
    base::Value child = EncodeNode(*node);
    children.Append(base::Value(std::move(child)));
  }
  base::Value dict(base::Value::Type::DICT);
  dict.GetDict().Set("format", base::Value("1"));
  dict.GetDict().Set("children", base::Value(std::move(children)));
  return dict;
}

base::Value TH_Codec::EncodeNode(const TH_Node& node) {
  base::Value src(base::Value::Type::DICT);
  src.GetDict().Set("code", base::Value(node.src().code));
  src.GetDict().Set("text", base::Value(node.src().text));
  base::Value translated(base::Value::Type::DICT);
  translated.GetDict().Set("code", base::Value(node.translated().code));
  translated.GetDict().Set("text", base::Value(node.translated().text));

  base::Value dict(base::Value::Type::DICT);
  dict.GetDict().Set("id", base::Value(node.id()));
  dict.GetDict().Set("src", base::Value(std::move(src)));
  dict.GetDict().Set("translated", base::Value(std::move(translated)));
  dict.GetDict().Set(
      "date_added",
      base::NumberToString(
          node.date_added().ToDeltaSinceWindowsEpoch().InMicroseconds()));

  return dict;
}

}  // namespace translate_history
