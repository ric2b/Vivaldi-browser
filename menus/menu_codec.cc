// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "menus/menu_codec.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>

#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "menus/menu_node.h"
#include "menus/menu_model.h"

namespace menus {

bool MenuCodec::GetVersion(std::string* version, const base::Value& value) {
  if (!value.is_list()) {
    LOG(ERROR) << "Menu Codec: No list";
    return false;
  }

  for (const auto& menu : value.GetList()) {
    if (menu.is_dict()) {
      const std::string* entry = menu.FindStringPath("version");
      if (entry) {
        *version = *entry;
        return true;
      }
    }
  }
  return false;
}

bool MenuCodec::Decode(Menu_Node* root, Menu_Control* control,
                       const base::Value& value, bool is_bundle,
                       const std::string& force_version) {
  if (!value.is_list()) {
    LOG(ERROR) << "Menu Codec: No list";
    return false;
  }
  for (const auto& menu : value.GetList()) {
    if (menu.is_dict()) {
      const std::string* type = menu.FindStringPath("type");
      const std::string* guid = menu.FindStringPath("guid");
      bool guid_valid = guid && !guid->empty() && base::IsValidGUID(*guid);

      if (guid_valid && type && *type == "menu") {
        const std::string* action = menu.FindStringPath("action");
        const std::string* role = menu.FindStringPath("role");
        if (action && role) {
          std::unique_ptr<Menu_Node> node = std::make_unique<Menu_Node>(*guid,
              Menu_Node::GetNewId());
          node->SetType(Menu_Node::MENU);
          node->SetRole(*role);
          node->SetAction(*action);
          // We add the menu even when there is no children added, but not
          // if parsing failed.
          bool parsing_ok = true;
          const base::Value* children = menu.FindPath("children");
          if (children && children->is_list()) {
            parsing_ok = DecodeNode(node.get(), *children, is_bundle);
          }
          if (parsing_ok) {
            root->Add(std::move(node));
          }
        } else {
          if (!role) {
            LOG(ERROR) << "Menu Codec: Role missing";
          }
          if (!action) {
            LOG(ERROR) << "Menu Codec: Action missing";
          }
        }
      } else if (type && *type == "control") {
        const std::string* format = menu.FindStringPath("format");
        const std::string* version = force_version.empty() ?
          menu.FindStringPath("version") : &force_version;
        const base::Value* deleted_list = menu.FindPath("deleted");
        if (format) {
          control->format = *format;
        }
        if (version) {
          control->version = *version;
        }
        if (deleted_list && deleted_list->is_list()) {
          for (const auto& deleted : deleted_list->GetList()) {
            control->deleted.push_back(deleted.GetString());
          }
        }
      } else {
        if (!guid_valid) {
          LOG(ERROR) << "Menu Codec: Guid missing or not valid";
#if !defined(OFFICIAL_BUILD)
          if (is_bundle) {
            LOG(ERROR) << "Menu Codec: Developer - Missing in bundled file, "
                          "add: " << base::GenerateGUID();
          } else {
            LOG(ERROR) << "Menu Codec: Developer - Missing in profile file, "
                          "remove that file.";
          }
#endif
        }
        if (!type) {
          LOG(ERROR) << "Menu Codec: Type missing";
        } else if (*type != "menu") {
          LOG(ERROR) << "Menu Codec: Unsupported type: " << type;
        }
      }
    } else {
      LOG(ERROR) << "Menu Codec: Wrong list format";
      return false;
    }
  }

  return true;
}

bool MenuCodec::DecodeNode(Menu_Node* parent, const base::Value& value,
                           bool is_bundle) {
  if (value.is_list()) {
    for (const auto& item : value.GetList()) {
      if (!DecodeNode(parent, item, is_bundle)) {
        return false;
      }
    }
  } else if (value.is_dict()) {
    const std::string* type = value.FindStringPath("type");
    const std::string* action = value.FindStringPath("action");
    const std::string* title = value.FindStringPath("title");
    const std::string* guid = value.FindStringPath("guid");
    const std::string* parameter = value.FindStringPath("parameter");
    const int origin = value.FindIntKey("origin").value_or(Menu_Node::BUNDLE);
    bool guid_valid = guid && !guid->empty() && base::IsValidGUID(*guid);

    if (type) {
      if (!guid_valid) {
        LOG(ERROR) << "Menu Codec: Guid missing or not valid for " << *type;
#if !defined(OFFICIAL_BUILD)
        if (is_bundle) {
          LOG(ERROR) << "Menu Codec: Developer - Missing in bundled file, "
                        "add: " << base::GenerateGUID();
        } else {
          LOG(ERROR) << "Menu Codec: Developer - Missing in profile file, "
                        "remove that file.";
        }
        return true;  // Do not stop parsing in devel mode.
#else
        return false;
#endif  // !OFFICIAL_BUILD
      }
      if (!action && *type != "separator") {
        LOG(ERROR) << "Menu Codec: Action missing for " << *type;
        return false;
      }
      std::unique_ptr<Menu_Node> node = std::make_unique<Menu_Node>(*guid,
          Menu_Node::GetNewId());
      node->SetAction(action ? *action : "");
      if (origin == Menu_Node::BUNDLE)
        node->SetOrigin(Menu_Node::BUNDLE);
      else if (origin == Menu_Node::MODIFIED_BUNDLE)
        node->SetOrigin(Menu_Node::MODIFIED_BUNDLE);
      else if (origin == Menu_Node::USER)
        node->SetOrigin(Menu_Node::USER);
      else {
        LOG(ERROR) << "Menu Codec: Uknown origin for " << *type;
        return false;
      }
      if (title) {
        node->SetTitle(base::UTF8ToUTF16(*title));
        node->SetHasCustomTitle(true);  // Even if title length is 0.
      }
      if (*type == "command") {
        node->SetType(Menu_Node::COMMAND);
        if (parameter) {
          node->SetParameter(*parameter);
        }
      } else if (*type == "checkbox") {
        node->SetType(Menu_Node::CHECKBOX);
      } else if (*type == "radio") {
        const std::string* radio_group = value.FindStringPath("radiogroup");
        if (!radio_group) {
          LOG(ERROR) << "Menu Codec: Radio group missing for " << action;
          return false;
        }
        node->SetRadioGroup(*radio_group);
        node->SetType(Menu_Node::RADIO);
      } else if (*type == "separator") {
        node->SetType(Menu_Node::SEPARATOR);
      } else if (*type == "folder") {
        node->SetType(Menu_Node::FOLDER);
        const base::Value* children_of_folder = value.FindPath("children");
        if (children_of_folder) {
          if (!DecodeNode(node.get(), *children_of_folder, is_bundle)) {
            return false;
          }
        }
      } else if (*type == "container") {
        const std::string* container_mode = value.FindStringPath("mode");
        const std::string* edge = value.FindStringPath("edge");
        if (!container_mode) {
          LOG(ERROR) << "Menu Codec: Container mode missing for " << action;
          return false;
        }
        if (*container_mode != "inline" && *container_mode != "folder") {
          LOG(ERROR) << "Menu Codec: Illegal container mode for " << action;
          return false;
        }
        // An edge is not set for all containers.
        if (edge) {
          if (*edge != "above" && *edge != "below" && *edge != "off") {
             LOG(ERROR) << "Menu Codec: Illegal container edge for " << action;
             return false;
          }
        }
        // The edge was defined after first official build so add a fallback.
        node->SetContainerEdge(edge ? *edge : "below");
        node->SetContainerMode(*container_mode);
        node->SetType(Menu_Node::CONTAINER);
      } else {
         LOG(ERROR) << "Menu Codec: Illegal type: " << type;
        return false;
      }
      parent->Add(std::move(node));
    } else {
       LOG(ERROR) << "Menu Codec: Type missing";
      return false;
    }
  } else {
     LOG(ERROR) << "Menu Codec: Illegal category";
    return false;
  }
  return true;
}

base::Value MenuCodec::Encode(Menu_Model* model) {
  base::Value list;
  if (model->mainmenu_node()) {
    list = EncodeNode(model->mainmenu_node());
  } else {
    list = base::Value(base::Value::Type::LIST);
  }

  Menu_Control* control = model->GetControl();
  if (control) {
    base::Value dict(base::Value::Type::DICTIONARY);
    base::Value deleted_list(base::Value::Type::LIST);
    for (const auto& deleted : control->deleted) {
      deleted_list.Append(deleted);
    }
    dict.SetKey("type", base::Value("control"));
    dict.SetKey("deleted", base::Value(std::move(deleted_list)));
    dict.SetKey("format", base::Value(control->format));
    dict.SetKey("version", base::Value(control->version));
    list.Append(base::Value(std::move(dict)));
  }

  return list;
}

base::Value MenuCodec::EncodeNode(Menu_Node* node) {
  if (node->id() == Menu_Node::mainmenu_node_id()) {
    base::Value list(base::Value::Type::LIST);
    for (const auto& child : node->children()) {
      list.Append(EncodeNode(child.get()));
    }
    return list;
  }

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("action", base::Value(node->action()));
  dict.SetKey("guid", base::Value(node->guid()));
  if (node->hasCustomTitle()) {
    dict.SetKey("title", base::Value(node->GetTitle()));
  }
  if (node->origin() != Menu_Node::BUNDLE) {
    dict.SetKey("origin", base::Value(node->origin()));
  }
  bool is_folder = false;
  switch (node->type()) {
    case Menu_Node::MENU:
      dict.SetKey("type", base::Value("menu"));
      dict.SetKey("role", base::Value(node->role()));
      is_folder = true;
      break;
    case Menu_Node::FOLDER:
      dict.SetKey("type", base::Value("folder"));
      is_folder = true;
      break;
    case Menu_Node::COMMAND:
      dict.SetKey("type", base::Value("command"));
      if (node->parameter().length() > 0) {
        dict.SetKey("parameter", base::Value(node->parameter()));
      }
      break;
    case Menu_Node::CHECKBOX:
      dict.SetKey("type", base::Value("checkbox"));
      break;
    case Menu_Node::RADIO:
      dict.SetKey("type", base::Value("radio"));
      dict.SetKey("radiogroup", base::Value(node->radioGroup()));
      break;
    case Menu_Node::SEPARATOR:
      dict.SetKey("type", base::Value("separator"));
      break;
    case Menu_Node::CONTAINER:
      dict.SetKey("type", base::Value("container"));
      dict.SetKey("mode", base::Value(node->containerMode()));
      dict.SetKey("edge", base::Value(node->containerEdge()));
      break;
    default:
      NOTREACHED();
      break;
  }

  if (is_folder) {
    base::Value list(base::Value::Type::LIST);
    for (const auto& child : node->children()) {
      list.Append(EncodeNode(child.get()));
    }
    dict.SetKey("children", base::Value(std::move(list)));
  }
  return dict;
}

}  // namespace vivaldi
