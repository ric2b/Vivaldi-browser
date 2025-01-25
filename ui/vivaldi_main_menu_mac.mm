//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//
#include "ui/vivaldi_main_menu_mac.h"

#include "app/vivaldi_commands.h"
#include "base/base64.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"  // For IDC_WINDOW_MENU
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#import "chrome/browser/app_controller_mac.h"
#include "components/favicon/core/favicon_service.h"
#include "extensions/schema/menubar.h"
#import "ui/base/l10n/l10n_util_mac.h"
#include "ui/vivaldi_bookmark_menu_mac.h"
#include "ui/vivaldi_main_menu.h"

namespace menubar = extensions::vivaldi::menubar;

namespace vivaldi {
void PopulateMenu(const menubar::MenuItem& item, NSMenu* menu, bool topLevel,
    long index, FaviconLoaderMac* faviconLoader, int min_tag, int max_tag);
}

// Menu delegate that supports replacing items with the specified tag that
// is defined in init. It is assumed all are listed one after another and that
// the section starts with a separator with tag value
// IDC_VIV_WINDOW_SEPARATOR_MAC
@interface MainMenuDelegate : NSObject<NSMenuDelegate> {
 @private
  std::vector<menubar::MenuItem> items_;
  bool has_new_content_;
  int tag_;
  int min_tag_;
  int max_tag_;
  std::unique_ptr<vivaldi::FaviconLoaderMac> favicon_loader_;
}

- (id)initWithProfile:(Profile*)profile tag:(int)tag
                                    min_tag:(int)min_tag
                                    max_tag:(int)max_tag;
- (void)setMenuItems:(std::vector<menubar::MenuItem>*)items;
- (void)menuNeedsUpdate:(NSMenu *)menu;
- (vivaldi::FaviconLoaderMac*)faviconLoader;

@end

@implementation MainMenuDelegate

- (id)initWithProfile:(Profile*)profile tag:(int)tag
                                    min_tag:(int)min_tag
                                    max_tag:(int)max_tag {
  self = [super init];
  if (self) {
    has_new_content_ = false;
    tag_ = tag;
    min_tag_ = min_tag;
    max_tag_ = max_tag;
    favicon_loader_.reset(new vivaldi::FaviconLoaderMac(profile));
  }
  return self;
}

- (void)dealloc {
  [self reset];
  // TODO: Test properly for problems wrt ARC transition
  //[super dealloc];
}

- (vivaldi::FaviconLoaderMac*)faviconLoader {
  return favicon_loader_.get();
}

- (void)reset {
  items_.clear();
  favicon_loader_->CancelPendingRequests();
  has_new_content_ = false;
}

- (void)setMenuItems:(std::vector<menubar::MenuItem>*)items {
  items_ = std::move(*items);
  has_new_content_ = true;
}

- (void)menuNeedsUpdate:(NSMenu *)menu {
  if (!has_new_content_)
    return;
  has_new_content_ = false;

  favicon_loader_->CancelPendingRequests();

  NSArray* items = [menu itemArray];
  for (NSMenuItem* item in items) {
    if ([item hasSubmenu]) {
    }
    if (item.tag == tag_ || [item hasSubmenu]) {
      [menu removeItem:item];
    }
  }

  // Locate the separator in front of the new tab items
  long startIndex = -1;
  for (long i=0; i<menu.numberOfItems; i++) {
    if ([menu itemAtIndex:i].tag == IDC_VIV_WINDOW_SEPARATOR_MAC) {
      startIndex = i;
      break;
    }
  }
  // Add all pending items
  if (startIndex != -1) {
    for (std::vector<menubar::MenuItem>::const_iterator it =
          items_.begin(); it != items_.end(); ++it) {
      const menubar::MenuItem& item = *it;
      vivaldi::PopulateMenu(item, menu, false, ++startIndex,
        favicon_loader_.get(), min_tag_, max_tag_);
    }
  }
}

@end

namespace vivaldi {

MainMenuDelegate* g_window_menu_delegate = nullptr;


// Responsible for loading a favicon using a url as key and assign it to the
// corresponding menu item.
FaviconLoaderMac::FaviconLoaderMac(Profile* profile)
  :favicon_service_(nullptr),
   profile_(profile) {
  cancelable_task_tracker_.reset(new base::CancelableTaskTracker);
}

FaviconLoaderMac::~FaviconLoaderMac() {
}

void FaviconLoaderMac::LoadFavicon(NSMenuItem *item, const std::string& url) {
  if (!favicon_service_) {
    favicon_service_ = FaviconServiceFactory::GetForProfile(profile_,
        ServiceAccessType::EXPLICIT_ACCESS);
    if (!favicon_service_)
      return;
  }

  favicon_base::FaviconImageCallback callback =
    base::BindOnce(&FaviconLoaderMac::OnFaviconDataAvailable,
        base::Unretained(this), item);

  favicon_service_->GetFaviconImageForPageURL(
      GURL(url),
      std::move(callback),
      cancelable_task_tracker_.get());
}

void FaviconLoaderMac::OnFaviconDataAvailable(NSMenuItem* item,
  const favicon_base::FaviconImageResult& image_result) {
  if (!image_result.image.IsEmpty())
    item.image = image_result.image.ToNSImage();
}

void FaviconLoaderMac::CancelPendingRequests() {
  cancelable_task_tracker_.reset(new base::CancelableTaskTracker);
}

void FaviconLoaderMac::UpdateProfile(Profile* profile) {
  if (profile_ != profile) {
    profile_ = profile;
    favicon_service_ = nullptr;
  }
}

// Shortcut parsing

// Assumes all lowercase
unichar NSFunctionKeyFromString(std::string& string) {
  // NSXXXFunctionKeys are defined in NSEvent.h
  unichar keyValue = 0;
  size_t length = string.length();
  if (length >= 2 && string[0] == 'F') {
    if (length == 2) {
      char value = string[1];
      if (value == '1')
        keyValue = NSF1FunctionKey;
      else if (value == '2')
        keyValue = NSF2FunctionKey;
      else if (value == '3')
        keyValue = NSF3FunctionKey;
      else if (value == '4')
        keyValue = NSF4FunctionKey;
      else if (value == '5')
        keyValue = NSF5FunctionKey;
      else if (value == '6')
        keyValue = NSF6FunctionKey;
      else if (value == '7')
        keyValue = NSF7FunctionKey;
      else if (value == '8')
        keyValue = NSF8FunctionKey;
      else if (value == '9')
        keyValue = NSF9FunctionKey;
    } else if (length == 3 && string[1] == '1') {
      char value = string[2];
      if (value == '0')
        keyValue = NSF10FunctionKey;
      else if (value == '1')
        keyValue = NSF11FunctionKey;
      else if (value == '2')
        keyValue = NSF12FunctionKey;
      else if (value == '3')
        keyValue = NSF13FunctionKey;
      else if (value == '4')
        keyValue = NSF14FunctionKey;
      else if (value == '5')
        keyValue = NSF15FunctionKey;
      else if (value == '6')
        keyValue = NSF16FunctionKey;
      else if (value == '7')
        keyValue = NSF17FunctionKey;
      else if (value == '8')
        keyValue = NSF18FunctionKey;
      else if (value == '9')
        keyValue = NSF19FunctionKey;
    } else if (length == 3 && string[1] == '2') {
      char value = string[2];
      if (value == '0')
        keyValue = NSF20FunctionKey;
      else if (value == '1')
        keyValue = NSF21FunctionKey;
      else if (value == '2')
        keyValue = NSF22FunctionKey;
      else if (value == '3')
        keyValue = NSF23FunctionKey;
      else if (value == '4')
        keyValue = NSF24FunctionKey;
      else if (value == '5')
        keyValue = NSF25FunctionKey;
      else if (value == '6')
        keyValue = NSF26FunctionKey;
      else if (value == '7')
        keyValue = NSF27FunctionKey;
      else if (value == '8')
        keyValue = NSF28FunctionKey;
      else if (value == '9')
        keyValue = NSF29FunctionKey;
    } else if (length == 3 && string[1] == '3') {
      char value = string[2];
      if (value == '0')
        keyValue = NSF30FunctionKey;
      else if (value == '1')
        keyValue = NSF31FunctionKey;
      else if (value == '2')
        keyValue = NSF32FunctionKey;
      else if (value == '3')
        keyValue = NSF33FunctionKey;
      else if (value == '4')
        keyValue = NSF34FunctionKey;
      else if (value == '5')
        keyValue = NSF35FunctionKey;
    }
  }
  // Not all NSXXXFunctionKey can be used. Every new entry must be verfied.
  else if (string == "Home")
    keyValue = NSHomeFunctionKey;
  else if (string == "Begin")
    keyValue = NSBeginFunctionKey;
  else if (string == "End")
    keyValue = NSEndFunctionKey;
  else if (string == "Pageup")
    keyValue = NSPageUpFunctionKey;
  else if (string == "Pagedown")
    keyValue = NSPageDownFunctionKey;
  else if (string == "Up")
    keyValue = NSUpArrowFunctionKey;
  else if (string == "Down")
    keyValue = NSDownArrowFunctionKey;
  else if (string == "Left")
    keyValue = NSLeftArrowFunctionKey;
  else if (string == "Right")
    keyValue = NSRightArrowFunctionKey;

  return keyValue;
}

// Expected format: [modifier[+modifier]+]accelerator
NSString* acceleratorFromString(std::string& string) {
  size_t index = string.find_last_of("+");
  // The extra index test is for shortcuts like "cmd +" (written as cmd++).
  std::string candidate = index == std::string::npos || index == 0 ?
      string : string.substr(string[index-1] == '+' ? index : index + 1);

  NSString* accelerator = nil;
  unichar functionKey = NSFunctionKeyFromString(candidate);
  if (functionKey)
    accelerator = [NSString stringWithCharacters:&functionKey length:1];
  else {
    // Some constants in Menus.h Maybe use that.
    char menuGlyph = 0;
    if (candidate == "Backspace")
      menuGlyph = 0x08;
    else if (candidate == "Tab")
      menuGlyph = 0x09;
    else if (candidate == "Enter")
      menuGlyph = 0x0D;
    else if (candidate == "Escape" || candidate == "Esc")
      menuGlyph = 0x1B;
    else if (candidate == "Space")
      menuGlyph = 0x20;
    else if (candidate == "Delete" || candidate == "Del")
      menuGlyph = NSDeleteCharacter;  // Defined in NSText.h

    if (menuGlyph)
      accelerator = [NSString stringWithFormat:@"%c", menuGlyph];
    else
      accelerator = base::SysUTF8ToNSString(candidate);
  }
  return accelerator;
}

// Expected format: [modifier[+modifier]+]accelerator
// Assumes sanitized format. A broken string like "alshift" will be accepted.
NSUInteger modifierMaskFromString(std::string& string) {
  size_t index = string.find_last_of("+");
  std::string candidate = index == std::string::npos ?
      "" : string.substr(0, index);

  NSUInteger modifierMask = 0;
  if (candidate.length() >= 3) {
    if (candidate.find("Cmd") != std::string::npos ||
        candidate.find("Meta") != std::string::npos)
      modifierMask |= NSEventModifierFlagCommand;
    if (candidate.find("Shift") != std::string::npos)
      modifierMask |= NSEventModifierFlagShift;
    if (candidate.find("Alt") != std::string::npos)
      modifierMask |= NSEventModifierFlagOption;
    if (candidate.find("Ctrl") != std::string::npos)
      modifierMask |= NSEventModifierFlagControl;
  }
  return modifierMask;
}

void setMenuItemBold(NSMenuItem* item, bool bold) {
  NSFont* font;
  if (bold) {
    NSFontManager* manager = [NSFontManager sharedFontManager];
    font = [manager convertFont:[NSFont menuFontOfSize:0]
                    toHaveTrait:NSBoldFontMask];
  } else {
    font = [NSFont menuFontOfSize:0];
  }

  // TODO: Test properly for problems wrt ARC transition
  NSString* __strong title = [item title];
  NSDictionary* fontAttribute =
    [NSDictionary dictionaryWithObjectsAndKeys:font, NSFontAttributeName, nil];
  NSMutableAttributedString* newTitle =
    [[NSMutableAttributedString alloc] initWithString:title
                                           attributes:fontAttribute];
  [item setAttributedTitle:newTitle];
}

NSMenuItem* MakeMenuItem(NSString* title, const std::string& shortcut, int tag) {
  std::string shortcut_copy;
  if (!shortcut.empty()) {
    shortcut_copy = shortcut;
  }
  NSEventModifierFlags modifiers = modifierMaskFromString(shortcut_copy);
  NSString* keyEquivalent = acceleratorFromString(shortcut_copy);
  NSMenuItem* menuItem = [[NSMenuItem alloc]
          initWithTitle:title
                 action:nil
          keyEquivalent:modifiers & NSEventModifierFlagShift ?
              keyEquivalent.uppercaseString : keyEquivalent.lowercaseString];
  [menuItem setKeyEquivalentModifierMask:modifiers];
  [menuItem setTag:tag];
  return menuItem;
}

// Adds item and any subitems to the given menu at the given index. Adds at end
// if index is negative. If the item has a url and the faviconLoader is defined
// a favicon will be assigned to the item if available.
void PopulateMenu(const menubar::MenuItem& item, NSMenu* menu, bool topLevel,
    long index, FaviconLoaderMac* faviconLoader, int min_tag, int max_tag) {
  // Reuse existing toplevel menu item (if any).
  NSMenuItem* menuItem = topLevel ? [menu itemWithTag:item.id] : nullptr;
  if (menuItem) {
    // Top level order may have changed.
    int item_index = [menu indexOfItem:menuItem];
    if (index != item_index) {
      [menu removeItemAtIndex:item_index];
      [menu insertItem:menuItem atIndex:index];
    }
    // We can hide top level entries with the visible flag
    if (item.visible.has_value()) {
      [menuItem setHidden:!item.visible.value()];
    }
  } else {
    // Either a sub menu item or a new top level item.
    if (item.type == menubar::ItemType::kSeparator) {
      menuItem = [NSMenuItem separatorItem];
      [menuItem setTag:item.id];
      if (index == -1) {
        [menu addItem:menuItem];
      } else {
        [menu insertItem:menuItem atIndex:index];
      }
    } else {
      menuItem = MakeMenuItem(base::SysUTF8ToNSString(item.name),
          item.shortcut.value_or(""), item.id);
      bool visible = item.visible.value_or(true);
      [menuItem setHidden:!visible];
      if (item.parameter.has_value()) {
        // This is used by the tab items (and other types if needed later) that
        // all share the same mac menu id. Information of what to do (like what
        // tab to activate is stored here).
        [menuItem setRepresentedObject:
            base::SysUTF8ToNSString(item.parameter.value())];
      } else if (item.url.has_value()) {
        // Some commands reply on a url when executed.
        [menuItem setRepresentedObject:
            base::SysUTF8ToNSString(item.url.value())];
      }
      if (item.emphasized.has_value() && item.emphasized.value()) {
        setMenuItemBold(menuItem, true);
      }
      if (item.checked.has_value() && item.checked.value()) {
        [menuItem setState:NSControlStateValueOn];
      }

      AppController* appController =
          static_cast<AppController*>([NSApp delegate]);

      if (!appController) {
        return;
      }

      switch (item.id) {
        case IDC_CONTENT_CONTEXT_UNDO:
        case IDC_CONTENT_CONTEXT_REDO:
        case IDC_CONTENT_CONTEXT_CUT:
        case IDC_CONTENT_CONTEXT_COPY:
        case IDC_CONTENT_CONTEXT_PASTE:
        case IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE:
        case IDC_CONTENT_CONTEXT_DELETE:
        case IDC_CONTENT_CONTEXT_SELECTALL:
        case IDC_HIDE_APP:
        case IDC_VIV_HIDE_OTHERS:
        case IDC_VIV_SHOW_ALL:
        case IDC_VIV_MAC_ZOOM:
        case IDC_VIV_MAC_ALL_WINDOWS_TO_FRONT:
          // Do nothing (which basically means std. behavior).
          break;
        case IDC_VIV_SHARE_MENU_MAC:
          [menuItem setTag:0];
          // TODO: Test properly for problems wrt ARC transition
          [menuItem setSubmenu:[[NSMenu alloc] initWithTitle:
              base::SysUTF8ToNSString(item.name)]];
          if ([appController respondsToSelector:@selector(vivInitShareMenu:)]) {
            [appController vivInitShareMenu:menuItem];
          }
          break;
        case IDC_VIV_MAC_SERVICES:
          [menuItem setTag:0];
          // TODO: Test properly for problems wrt ARC transition
          [menuItem setSubmenu:[[NSMenu alloc] initWithTitle:
              base::SysUTF8ToNSString(item.name)]];
          [NSApp setServicesMenu :[menuItem submenu]];
          // Otherwise, as above, do nothing.
          break;
        case IDC_VIV_BOOKMARK_CONTAINER:
          // The container element is used to hand over information to chrome
          // bookmark generation code. So we hide the element itself.
          [menuItem setHidden:true];
          break;
        default:
          [menuItem setTarget:appController];
          break;
      }
      if ([appController
          respondsToSelector:@selector(setVivaldiMenuItemAction:)]) {
        [appController setVivaldiMenuItemAction:menuItem];
      }

      if (item.icon.has_value() && item.icon.value().length() > 0) {
        std::string png_data;
        if (base::Base64Decode(*item.icon, &png_data)) {
          gfx::Image img = gfx::Image::CreateFrom1xPNGBytes(
            base::span(reinterpret_cast<const unsigned char*>(png_data.c_str()),
            png_data.length()));
          menuItem.image = img.ToNSImage();
        }
      }
      if (faviconLoader && item.url.has_value() && item.url.value().length() > 0)
        faviconLoader->LoadFavicon(menuItem, item.url.value());

      if (index == -1) {
        [menu addItem:menuItem];
      } else {
        [menu insertItem:menuItem atIndex:index];
      }
    }
  }

  if (menuItem && item.items) {
    long index2 = -1;
    NSMenu* subMenu;
    if ([menuItem hasSubmenu]) {
      subMenu = [menuItem submenu];
      // We may change the string on each update.
      [subMenu setTitle:base::SysUTF8ToNSString(item.name)];
      switch (item.id) {
        case IDC_EDIT_MENU:
          for (NSMenuItem* item2 in [subMenu itemArray]) {
            if (item2.tag == IDC_SPELLCHECK_MENU ||
                item2.tag == IDC_VIV_SUBSTITUTIONS_MENU_MAC ||
                item2.tag == IDC_VIV_SPEECH_MENU_MAC ||
                item2.tag == IDC_VIV_EDIT_SEPARATOR_MAC) {
              // Set up in vivaldi_main_menu_buider.mm It seems we have to do
              // it there to get it working. This means this submenu with
              // content is not configurable.
              continue;
            } else if (item2.action ==
                       NSSelectorFromString(@"orderFrontCharacterPalette:") ||
                       item2.action ==
                       NSSelectorFromString(@"startDictation:")) {
              // These two are added by AppKit at end of the menu and we leave
              // them alone.
              continue;
            }
            [subMenu removeItem:item2];
          }
          index2 = 0; // Our items always start from the top of the menu.
          break;
        case IDC_BOOKMARKS_MENU:
          // Special care for the bookmarks menu as we manage it two places.
          // Here and in bookmark_menu_bridge.mm. Remove all items that have
          // been created earlier by this function.
          {
            NSMenu* saved_bookmark_menu = GetBookmarkMenu();
            if (subMenu != saved_bookmark_menu) {
              // The menu itself has changed. Typically happens when we switch
              // to/from a guest window. Our caching of menu ids will not work
              // so we clear the entire menu. We prefer to avoid this as there
              // can be many bookmarks.
              // Bookmarks (all bookmarks and items created by the container).
              ClearBookmarkMenu();
              // All regular items (configurable from UI).
              [subMenu removeAllItems];
              // Register new menu.
              SetBookmarkMenu(subMenu);
            } else {
              for (auto id: GetBookmarkMenuIds()) {
                NSMenuItem* installed_item = [subMenu itemWithTag:id];
                if (installed_item) {
                  long installed_index = [subMenu indexOfItem:installed_item];
                  [subMenu removeItemAtIndex:installed_index];
                }
              }
            }
            // Clear cached ids
            GetBookmarkMenuIds().clear();
            // New items are added a bit further down in the function.
          }
          break;
        case IDC_WINDOW_MENU:
          // Special hardcoding for COMMAND_WINDOW_MINIMIZE which is mapped to
          // IDC_VIV_MAC_MINIMIZE in menubar_api.cc due to random multiple
          // minimize calls on some systems. Hardcoding here avoids a dummy
          // element in mainmenu.json which would show up alongside in all the
          // other element in Settings UI that can be configured which
          // this one can not (adding it in vivaldi_main_menu_builder.mm fails
          // as well).
          {
            NSMenuItem* item3 = [subMenu numberOfItems] > 0
              ? [subMenu itemAtIndex:0] : nullptr;
            if (!item3 || item3.tag != IDC_VIV_MAC_MINIMIZE) {
              std::string shortcut = "Meta+M";
              item3 = MakeMenuItem(
                l10n_util::GetNSStringWithFixup(IDS_MINIMIZE_WINDOW_MAC),
                shortcut,
                IDC_VIV_MAC_MINIMIZE);
              AppController* appController =
                  static_cast<AppController*>([NSApp delegate]);
              [item3 setTarget:appController];
              [subMenu insertItem:item3 atIndex:0];
              if ([appController
                respondsToSelector:@selector(setVivaldiMenuItemAction:)]) {
                [appController setVivaldiMenuItemAction:item3];
              }
            }
          }

          // This menu has issues on macOS 10.15+ (VB-58755)
          // We remove all items we have added ourself and start adding after
          // the "magic" separator with the tag value
          // IDC_VIV_WINDOW_SEPARATOR_MAC (see vivaldi_main_menu_builder.mm).
          // This means we can not configure all of this menu.
          index2 = [subMenu indexOfItem:
              [subMenu itemWithTag:IDC_VIV_WINDOW_SEPARATOR_MAC]];
          for (NSMenuItem* item5 in [subMenu itemArray]) {
            // MacOS adds a window list at bottom. Items are tagged with 0.
            if ([subMenu indexOfItem:item5] > index2 && item5.tag != 0) {
              [subMenu removeItem:item5];
            }
          }
          index2 ++; // Start after the separator
          break;
        default:
          [subMenu removeAllItems];
          break;
      }
    } else {
      NSString* title = base::SysUTF8ToNSString(item.name);
      // TODO: Test properly for problems wrt ARC transition
      subMenu = [[NSMenu alloc] initWithTitle:title];
      [menuItem setSubmenu:subMenu];
    }

    if (item.id == IDC_BOOKMARKS_MENU) {
      // Special care for the bookmarks menu as we manage it two places. Here
      // and in bookmark_menu_bridge.mm. We only add UI menu items below.

      // The menu currently only holds elements added in bookmark_menu_bridge.mm
      // (it may be none as the menu is popuplated there when it is about to be
      // shown). Those represent the content of IDC_VIV_BOOKMARK_CONTAINER. The
      // other items are added here and are regular menu items.
      int num_installed_bookmarks = subMenu.numberOfItems;
      index2 = 0;
      for (const menubar::MenuItem& child: *item.items) {
        GetBookmarkMenuIds().push_back(child.id);
        PopulateMenu(child, subMenu, false, index2, faviconLoader, 0, 0);
        if (child.id == IDC_VIV_BOOKMARK_CONTAINER) {
          // Register container and its offset.
          SetContainerState(extensions::vivaldi::menubar::ToString(child.edge),
            index2);
          // Step over installed bookmarks.
          index2 += num_installed_bookmarks;
        }
        index2 ++;
      }
    } else {
      for (const menubar::MenuItem& child: *item.items) {
        PopulateMenu(child, subMenu, false, index2, faviconLoader, min_tag,
                    max_tag);
        if (index2 != -1) {
          index2 ++;
        }
      }
    }

  }
}

void UpdateMenuItem(const menubar::MenuItem& item, NSMenuItem* menuItem) {
  if (item.checked.has_value()) {
    [menuItem setState:
        item.checked.value() ? NSControlStateValueOn : NSControlStateValueOff];
  }
  if (item.emphasized.has_value()) {
    setMenuItemBold(menuItem, item.emphasized.value() ? true : false);
  }
  bool visible = item.visible.value_or(true);
  [menuItem setHidden:!visible];
}

NSMenuItem* GetMenuItemByTag(NSMenu* menu, int tag) {
  for (NSMenuItem* menuItem in menu.itemArray) {
    if (menuItem.tag == tag)
      return menuItem;
    if (menuItem.hasSubmenu) {
      NSMenuItem* tmp = GetMenuItemByTag(menuItem.submenu, tag);
      if (tmp)
        return tmp;
    }
  }
  return nullptr;
}

void CreateVivaldiMainMenu(
    Profile* profile,
    std::vector<menubar::MenuItem>* items,
    int min_id,
    int max_id) {

  NSMenu* mainMenu = [NSApp mainMenu];
  NSMenuItem* windowMenuItem = GetMenuItemByTag(mainMenu, IDC_WINDOW_MENU);

  if (!g_window_menu_delegate) {
    if ([windowMenuItem hasSubmenu]) {
      g_window_menu_delegate =
        [[MainMenuDelegate alloc] initWithProfile:profile
                                              tag:IDC_VIV_ACTIVATE_TAB
                                              min_tag:min_id
                                              max_tag:max_id];
      windowMenuItem.submenu.delegate = g_window_menu_delegate;
    }
  } else {
    // Ensure correct profile. We use a service that depends on the profile.
    // Should the profile be removed the service will no longer be valid.
    [g_window_menu_delegate faviconLoader]->UpdateProfile(profile);
  }

  // Full update. Remove any pending requests.
  [g_window_menu_delegate reset];

  FaviconLoaderMac* faviconLoader = [g_window_menu_delegate faviconLoader];
  int index = 0;
  for (const menubar::MenuItem& item : *items) {
    PopulateMenu(item, mainMenu, true, index++, faviconLoader, min_id,
                 max_id);
  }
  // In PopulateMenu we never remove any existing top level items so we do
  // that here in case configuration has removed one or more.
  for (int menubarIndex = 0; menubarIndex < [mainMenu numberOfItems]; ) {
    NSMenuItem* menuItem = [mainMenu itemAtIndex:menubarIndex];
    bool present = false;
    for (const menubar::MenuItem& item : *items) {
      if (item.id == menuItem.tag) {
        present = true;
        break;
      }
    }
    if (!present) {
      [mainMenu removeItem:menuItem];
    } else {
      menubarIndex ++;
    }
  }

  // macOS will sometimes add the alternateArrangeInFront menuitem.
  // Remove it if we find it.
  if ([windowMenuItem hasSubmenu]) {
    for (NSMenuItem* item in [[windowMenuItem submenu] itemArray]) {
      if ([item action] == NSSelectorFromString(@"alternateArrangeInFront:")) {
        [[windowMenuItem submenu] removeItem:item];
      }
    }
  }
}

}  // namespace vivaldi
