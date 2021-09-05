// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef MENUS_MENU_CODEC_H_
#define MENUS_MENU_CODEC_H_

#include <set>
#include <string>

#include "base/macros.h"

namespace base {
class Value;
}  // namespace base

namespace menus {
struct Menu_Control;
class Menu_Model;
class Menu_Node;
}  // namespace menus

namespace menus {

// Decodes JSON values at a Menu_Model and encodes a Menu_Model into JSON.
class MenuCodec {
 public:
  MenuCodec() = default;
  ~MenuCodec() = default;

  // Looks up and returns the file version.
  bool GetVersion(std::string* version, const base::Value& value);

  // Decodes JSON into a Menu_Model object. Returns true on success,
  // false otherwise.
  bool Decode(Menu_Node* root, Menu_Control* control, const base::Value& value,
              bool is_bundle);

  // Encodes the model to a corresponding JSON value tree.
  base::Value Encode(Menu_Model* model);

 private:
  bool DecodeNode(Menu_Node* parent, const base::Value& value, bool is_bundle);
  base::Value EncodeNode(Menu_Node* node);

  DISALLOW_COPY_AND_ASSIGN(MenuCodec);
};

}  // namespace menus

#endif  // MENUS_MENU_CODEC_H_
