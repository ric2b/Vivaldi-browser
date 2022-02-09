// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef MENUS_MENU_CODEC_H_
#define MENUS_MENU_CODEC_H_

#include <map>
#include <set>
#include <string>

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
  MenuCodec();
  ~MenuCodec();
  MenuCodec(const MenuCodec&) = delete;
  MenuCodec& operator=(const MenuCodec&) = delete;

  // Looks up and returns the file version.
  bool GetVersion(std::string* version, const base::Value& value);

  // Decodes JSON into a Menu_Model object. Returns true on success,
  // false otherwise.
  bool Decode(Menu_Node* root,
              Menu_Control* control,
              const base::Value& value,
              bool is_bundle,
              const std::string& force_version);

  // Encodes the model to a corresponding JSON value tree.
  base::Value Encode(Menu_Model* model);

 private:
  typedef std::map<std::string, bool> StringToBoolMap;
  bool DecodeNode(Menu_Node* parent, const base::Value& value, bool is_bundle);
  base::Value EncodeNode(Menu_Node* node);

  StringToBoolMap guids_;
};

}  // namespace menus

#endif  // MENUS_MENU_CODEC_H_
