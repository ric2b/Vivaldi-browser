//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#include "browser/menus/vivaldi_context_menu_controller.h"

#include "base/base64.h"
#include "base/strings/utf_string_conversions.h"
#include "browser/vivaldi_browser_finder.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon/core/favicon_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/context_menu_params.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/vivaldi_context_menu.h"

namespace vivaldi {

ContextMenuController::ContextMenuController(
    Delegate* delegate,
    content::WebContents* web_contents,
    content::WebContents* window_web_contents,
    std::unique_ptr<Params> params)
  :delegate_(delegate),
   web_contents_(web_contents),
   window_web_contents_(window_web_contents),
   browser_(FindBrowserForEmbedderWebContents(web_contents)),
   params_(std::move(params)) {

  using Origin = extensions::vivaldi::context_menu::Origin;

  // We set the height to zero depending on where we want chrome code to
  // position the menu. Chrome does not support placing a menu to the right or
  // left of the "forbidden zone" (the rect we set up here), so for those
  // configuations eg, a stack menu from a tab stack in a vertical bar, we have
  // SetPosition() below which is called from chrome after the menu size is
  // known.
  gfx::Point point(params_->properties.rect.x, params_->properties.rect.y);
  int height = params_->properties.rect.height;
  if (params_->properties.origin == Origin::ORIGIN_TOPLEFT ||
      params_->properties.origin == Origin::ORIGIN_TOPRIGHT) {
    height = 0;
  }
  int width = params_->properties.rect.width;

  gfx::RectF rect(point.x(), point.y(), width, height);
  FromUICoordinates(window_web_contents_, &rect);
  rect_ = gfx::Rect(round(rect.x()), round(rect.y()), round(rect.width()),
                    round(rect.height()));

  developertools_controller_.reset(
      new DeveloperToolsMenuController(window_web_contents_, rect_.origin()));
}

ContextMenuController::~ContextMenuController() {
}

void ContextMenuController::Show() {
  using Origin = extensions::vivaldi::context_menu::Origin;

  InitModel();
  delegate_->OnOpened();
  // We do not know if count is 0 until after InitModel, but let OnOpened and
  // OnClosed be called as normal.
  if (menu_model_->GetItemCount() == 0) {
    MenuClosed(menu_model_);
    return;
  }

  // Mac needs the views version for certain origins as we can not place the
  // menu properly on mac/cocoa.
  bool force_views = params_->properties.origin != Origin::ORIGIN_POINTER;
  menu_.reset(CreateVivaldiContextMenu(window_web_contents_, menu_model_, rect_,
                                       force_views,
                                       force_views ? this : nullptr));
  menu_->Show();
}

void ContextMenuController::InitModel() {
  namespace context_menu = extensions::vivaldi::context_menu;

  menu_model_ = new ui::SimpleMenuModel(this);
  models_.push_back(base::WrapUnique(menu_model_));

  // Add items from JS
  for (const context_menu::Element& child: params_->properties.children) {
    PopulateModel(child, menu_model_);
  }

  // Add developer tools items
  developertools_controller_->PopulateModel(menu_model_);

  SanitizeModel(menu_model_);
}

void ContextMenuController::PopulateModel(const Element& child,
                                          ui::SimpleMenuModel* menu_model) {
  namespace context_menu = extensions::vivaldi::context_menu;
  if (child.item) {
    const Item& item = *child.item;
    int id = item.id + IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST + 1;
    const base::string16 label = base::UTF8ToUTF16(item.name);

    switch (item.type) {
      case context_menu::ITEM_TYPE_COMMAND:
        menu_model->AddItem(id, label);
        break;
      case context_menu::ITEM_TYPE_CHECKBOX:
        menu_model->AddCheckItem(id, label);
        id_to_checked_map_[id] = item.checked && *item.checked;
        break;
      case context_menu::ITEM_TYPE_RADIO:
        menu_model->AddRadioItem(id, label, *item.radiogroup.get());
        id_to_checked_map_[id] = item.checked && *item.checked;
        break;
      case context_menu::ITEM_TYPE_FOLDER:
        {
          ui::SimpleMenuModel* child_menu_model = new ui::SimpleMenuModel(this);
          models_.push_back(base::WrapUnique(child_menu_model));

          menu_model->AddSubMenu(id, label, child_menu_model);
          for (const Element& it: *child.children) {
            PopulateModel(it, child_menu_model);
          }
          SanitizeModel(child_menu_model);
        }
        break;
      case context_menu::ITEM_TYPE_NONE:
        return;
    }
    if (item.shortcut) {
      id_to_accelerator_map_[id] = ::vivaldi::ParseShortcut(
          *item.shortcut, true);
    }
    if (item.url && !item.url->empty()) {
       // Set default document icon
      SetIcon(id, params_->properties.icons.at(0), menu_model);
      // Attempt loading a favicon that will replace the default.
      id_to_url_map_[id] = item.url.get();
      LoadFavicon(id, *item.url);
    } else if (item.icons && item.icons->size() == 2) {
      // Fixed for now. Using same format as main menus api.
      bool dark_text_color = true;
      SetIcon(id, item.icons->at(dark_text_color ? 0 : 1), menu_model);
    }
  } else if (child.container) {
    const Container& container = *child.container;
    switch (container.content) {
      case context_menu::CONTAINER_CONTENT_PWA:
        pwa_controller_.reset(new PWAMenuController(web_contents_));
        pwa_controller_->PopulateModel(GetContainerModel(container, menu_model));
        break;
      case context_menu::CONTAINER_CONTENT_NONE:
        break;
    }
  } else if (child.separator) {
    menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
  }
}

ui::SimpleMenuModel* ContextMenuController::GetContainerModel(
    const Container& container, ui::SimpleMenuModel* menu_model) {
  namespace context_menu = extensions::vivaldi::context_menu;

  if (container.mode == context_menu::CONTAINER_MODE_FOLDER) {
    int id = container.id + IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST + 1;
    const base::string16 label = base::UTF8ToUTF16(container.name);
    ui::SimpleMenuModel* child_menu_model = new ui::SimpleMenuModel(this);
    models_.push_back(base::WrapUnique(child_menu_model));
    menu_model->AddSubMenu(id, label, child_menu_model);
    return child_menu_model;
  } else {
    return menu_model;
  }
}

void ContextMenuController::SanitizeModel(ui::SimpleMenuModel* menu_model) {
  for (int i = menu_model->GetItemCount() - 1; i >= 0; i--) {
    if (menu_model->GetTypeAt(i) == ui::MenuModel::TYPE_SEPARATOR) {
      menu_model->RemoveItemAt(i);
    } else {
      break;
    }
  }
}

// Called from chrome when menu size is known.
void ContextMenuController::SetPosition(gfx::Rect* menu_bounds,
                                        const gfx::Rect& monitor_bounds,
                                        const gfx::Rect& anchor_bounds) const {
  using Origin = extensions::vivaldi::context_menu::Origin;

  if (params_->properties.origin == Origin::ORIGIN_TOPRIGHT) {
    // Place left edge of menu to the right of anchor area. If not enough room
    // to fit inside monitor area move it to the left of the anchor area.
    menu_bounds->set_x(anchor_bounds.right());
    menu_bounds->set_y(anchor_bounds.bottom());
    if (menu_bounds->right() > monitor_bounds.right()) {
      menu_bounds->set_x(anchor_bounds.x() - menu_bounds->width());
    }
  } else if (params_->properties.origin == Origin::ORIGIN_TOPLEFT) {
    // Place right edge of menu to the left of anchor area. If not enough room
    // to fit inside monitor area move it to the right of the anchor area.
    menu_bounds->set_x(anchor_bounds.x() - menu_bounds->width());
    menu_bounds->set_y(anchor_bounds.bottom());
    if (menu_bounds->x() < monitor_bounds.x()) {
      menu_bounds->set_x(anchor_bounds.right());
    }
  }

  // Fallback code in chrome will ensure the menu is within the monitor area so
  // we do not test more than the last adjustment above.
}

void ContextMenuController::SetIcon(int command_id, const std::string& icon,
                                    ui::SimpleMenuModel* menu_model) {
  if (icon.length() > 0) {
    std::string png_data;
    if (base::Base64Decode(icon, &png_data)) {
      gfx::Image img = gfx::Image::CreateFrom1xPNGBytes(
          reinterpret_cast<const unsigned char*>(png_data.c_str()),
          png_data.length());
      menu_model->SetIcon(menu_model->GetIndexOfCommandId(command_id),
                          ui::ImageModel::FromImage(img));
    }
  }
}

void ContextMenuController::LoadFavicon(int command_id,
                                        const std::string& url) {
  if (!favicon_service_) {
    favicon_service_ = FaviconServiceFactory::GetForProfile(
        browser_->profile(), ServiceAccessType::EXPLICIT_ACCESS);
    if (!favicon_service_)
      return;
  }

  favicon_base::FaviconImageCallback callback =
      base::Bind(&ContextMenuController::OnFaviconDataAvailable,
                 base::Unretained(this), command_id);

  favicon_service_->GetFaviconImageForPageURL(GURL(url), std::move(callback),
                                              &cancelable_task_tracker_);
}

void ContextMenuController::OnFaviconDataAvailable(
    int command_id,
    const favicon_base::FaviconImageResult& image_result) {
  if (!image_result.image.IsEmpty()) {
    // We do not update the model. The MenuItemView class we use to paint the
    // menu does not support dynamic updates of icons through the model. We have
    // to set it directly.
    menu_->SetIcon(image_result.image, command_id);
  }
}

bool ContextMenuController::IsCommandIdChecked(int command_id) const {
  auto it = id_to_checked_map_.find(command_id);
  return it != id_to_checked_map_.end() ? it->second : false;
}

bool ContextMenuController::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool ContextMenuController::IsItemForCommandIdDynamic(int command_id) const {
  if (pwa_controller_ &&
      pwa_controller_->IsItemForCommandIdDynamic(command_id)) {
    return true;
  }
  return false;
}

// This function only needs to return a valid string for dynamic items.
base::string16 ContextMenuController::GetLabelForCommandId(
    int command_id) const {
  if (pwa_controller_ &&
      pwa_controller_->IsItemForCommandIdDynamic(command_id)) {
    return pwa_controller_->GetLabelForCommandId(command_id);
  }
  return base::string16();
}

// We do not specify accelerators in context menus in JS so mostly return false.
bool ContextMenuController::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  auto it = id_to_accelerator_map_.find(command_id);
  if (it != id_to_accelerator_map_.end()) {
    *accelerator = it->second;
    return true;
  }

  if (developertools_controller_->GetAcceleratorForCommandId(command_id,
                                                             accelerator)) {
    return true;
  }
  return false;
}

void ContextMenuController::VivaldiCommandIdHighlighted(int command_id) {
   auto it = id_to_url_map_.find(command_id);
   delegate_->OnHover(it != id_to_url_map_.end() ? *it->second : "");
}

void ContextMenuController::ExecuteCommand(int command_id, int event_flags) {
  if (developertools_controller_->HandleCommand(command_id)) {
  } else if (pwa_controller_ && pwa_controller_->HandleCommand(command_id)) {
  } else {
    delegate_->OnAction(command_id, event_flags);
  }
}

void ContextMenuController::MenuClosed(ui::SimpleMenuModel* source) {
  source->SetMenuModelDelegate(nullptr);
  if (source == menu_model_) {
    // TODO(espen): Closing by clicking outside the menu triggers a crash on
    // Mac. It seems to be access to data after a "delete this" which the
    // OnClosed call to the delegate starts, but the crash log is hard to make
    // sense of.
    timer_.reset(new base::OneShotTimer());
    timer_->Start(FROM_HERE, base::TimeDelta::FromMilliseconds(1),
        base::Bind(&ContextMenuController::DelayedClose, base::Unretained(
            this)));
  }
}

void ContextMenuController::DelayedClose() {
  delegate_->OnClosed();
  // We may now be deleted
}

}  // namespace vivaldi
