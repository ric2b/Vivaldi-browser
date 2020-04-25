//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//
#include "ui/vivaldi_main_menu_mac.h"

#include "app/vivaldi_commands.h"
#include "base/base64.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/app/chrome_command_ids.h"  // For IDC_WINDOW_MENU
#import "chrome/browser/app_controller_mac.h"
#include "components/favicon/core/favicon_service.h"
#include "ui/vivaldi_main_menu.h"

// This tag value must be used in the js code setting up the menus.
const int kSeparatorTag = 55555;

namespace menubar = extensions::vivaldi::menubar;

namespace vivaldi {
void PopulateMenu(const menubar::MenuItem& item, NSMenu* menu, bool topLevel,
    long index, FaviconLoaderMac* faviconLoader);
}

// Menu delegate that supports replacing items with the specified tag that
// is defined in init. It is assumed all are listed one after another and that
// the section starts with a separator with tag value kSeparatorTag
@interface MainMenuDelegate : NSObject<NSMenuDelegate> {
 @private
  std::vector<menubar::MenuItem> items_;
  bool has_new_content_;
  int tag_;
  std::unique_ptr<vivaldi::FaviconLoaderMac> favicon_loader_;
}

- (id)initWithProfile:(Profile*)profile tag:(int)tag;
- (void)setMenuItems:(std::vector<menubar::MenuItem>*)items;
- (void)menuNeedsUpdate:(NSMenu *)menu;
- (vivaldi::FaviconLoaderMac*)faviconLoader;

@end

@implementation MainMenuDelegate

- (id)initWithProfile:(Profile*)profile tag:(int)tag {
  self = [super init];
  if (self) {
    has_new_content_ = false;
    tag_ = tag;
    favicon_loader_.reset(new vivaldi::FaviconLoaderMac(profile));
  }
  return self;
}

- (void)dealloc {
  [self reset];
  [super dealloc];
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
    if ([menu itemAtIndex:i].tag == kSeparatorTag) {
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
        favicon_loader_.get());
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

// Shortcut parsing

// Assumes all lowercase
unichar NSFunctionKeyFromString(std::string& string) {
  // NSXXXFunctionKeys are defined in NSEvent.h
  unichar keyValue = 0;
  size_t length = string.length();
  if (length >= 2 && string[0] == 'f') {
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
  else if (string == "home")
    keyValue = NSHomeFunctionKey;
  else if (string == "begin")
    keyValue = NSBeginFunctionKey;
  else if (string == "end")
    keyValue = NSEndFunctionKey;
  else if (string == "pageup")
    keyValue = NSPageUpFunctionKey;
  else if (string == "pagedown")
    keyValue = NSPageDownFunctionKey;
  else if (string == "up")
    keyValue = NSUpArrowFunctionKey;
  else if (string == "own")
    keyValue = NSDownArrowFunctionKey;
  else if (string == "left")
    keyValue = NSLeftArrowFunctionKey;
  else if (string == "right")
    keyValue = NSRightArrowFunctionKey;

  return keyValue;
}

// Expected format: [modifier[+modifier]+]accelerator
NSString* acceleratorFromString(std::string& string) {
  size_t index = string.find_last_of("+");
  std::string candidate = index == std::string::npos ?
      string : string.substr(index+1);

  NSString* accelerator = nil;
  unichar functionKey = NSFunctionKeyFromString(candidate);
  if (functionKey)
    accelerator = [NSString stringWithCharacters:&functionKey length:1];
  else {
    // Some constants in Menus.h Maybe use that.
    char menuGlyph = 0;
    if (candidate == "backspace")
      menuGlyph = 0x08;
    else if (candidate == "tab")
      menuGlyph = 0x09;
    else if (candidate == "enter")
      menuGlyph = 0x0D;
    else if (candidate == "escape" || candidate == "esc")
      menuGlyph = 0x1B;
    else if (candidate == "Space")
      menuGlyph = 0x20;
    else if (candidate == "delete" || candidate == "del")
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
    if (candidate.find("cmd") != std::string::npos ||
        candidate.find("meta") != std::string::npos)
      modifierMask |= NSCommandKeyMask;
    if (candidate.find("shift") != std::string::npos)
      modifierMask |= NSShiftKeyMask;
    if (candidate.find("alt") != std::string::npos)
      modifierMask |= NSAlternateKeyMask;
    if (candidate.find("ctrl") != std::string::npos)
      modifierMask |= NSControlKeyMask;
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

  NSString* title = [item title];
  NSDictionary* fontAttribute =
    [NSDictionary dictionaryWithObjectsAndKeys:font, NSFontAttributeName, nil];
  NSMutableAttributedString* newTitle =
    [[NSMutableAttributedString alloc] initWithString:title
                                           attributes:fontAttribute];
  [item setAttributedTitle:newTitle];
  [newTitle release];
}

// Adds item and any subitems to the given menu at the given index. Adds at end
// if index is negative. If the item has a url and the faviconLoader is defined
// a favicon will be assigned to the item if available.
void PopulateMenu(const menubar::MenuItem& item, NSMenu* menu, bool topLevel,
    long index, FaviconLoaderMac* faviconLoader) {
  // Reuse existing toplevel menu item (if any). Assumes the string will
  // never change.
  NSMenuItem* menuItem = topLevel ? [menu itemWithTag:item.id] : nullptr;
  if (!menuItem) {
    if (item.name == "---") {
      NSMenuItem* menuItem = [NSMenuItem separatorItem];
      [menuItem setTag:item.id];
      if (index == -1) {
        [menu addItem:menuItem];
      } else {
        [menu insertItem:menuItem atIndex:index];
      }
    } else {
      bool visible = item.visible ? *item.visible : true;
      std::string shortcut;
      if (item.shortcut) {
        shortcut = *item.shortcut.get();
      }
      NSEventModifierFlags modifiers = modifierMaskFromString(shortcut);
      NSString* keyEquivalent = acceleratorFromString(shortcut);
      menuItem = [[NSMenuItem alloc]
          initWithTitle:base::SysUTF8ToNSString(item.name)
                 action:nil
          keyEquivalent:modifiers & NSShiftKeyMask ?
              keyEquivalent.uppercaseString : keyEquivalent.lowercaseString];
      [menuItem setKeyEquivalentModifierMask:modifiers];
      [menuItem setHidden:!visible];
      [menuItem setTag:item.id];
      if (item.parameter) {
        // This is used by the tab items (and other types if needed later) that
        // all share the same mac menu id. Information of what to do (like what
        // tab to activate is stored here).
        [menuItem setRepresentedObject:
            base::SysUTF8ToNSString(*item.parameter.get())];
      }
      if (item.emphasized && *item.emphasized.get()) {
        setMenuItemBold(menuItem, true);
      }
      if (item.checked && *item.checked.get()) {
        [menuItem setState:NSOnState];
      }

      AppController* appController =
          static_cast<AppController*>([NSApp delegate]);
      if (item.id != IDC_EXIT) {
        if (
            item.id != IDC_CONTENT_CONTEXT_UNDO &&
            item.id != IDC_CONTENT_CONTEXT_REDO &&
            item.id != IDC_CONTENT_CONTEXT_CUT &&
            item.id != IDC_CONTENT_CONTEXT_COPY &&
            item.id != IDC_CONTENT_CONTEXT_PASTE &&
            item.id != IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE &&
            item.id != IDC_CONTENT_CONTEXT_DELETE &&
            item.id != IDC_CONTENT_CONTEXT_SELECTALL &&
            item.id != IDC_HIDE_APP &&
            item.id != IDC_VIV_HIDE_OTHERS
        ) {
          [menuItem setTarget:appController];
        }
      } else {
        [menuItem setTarget:NSApp];
      }
      [appController setVivaldiMenuItemAction:menuItem];

      if (item.icon.get() && item.icon->length() > 0) {
        std::string png_data;
        if (base::Base64Decode(*item.icon, &png_data)) {
          gfx::Image img = gfx::Image::CreateFrom1xPNGBytes(
            reinterpret_cast<const unsigned char*>(png_data.c_str()),
            png_data.length());
          menuItem.image = img.ToNSImage();
        }
      }
      if (faviconLoader && item.url.get() && item.url->length() > 0)
        faviconLoader->LoadFavicon(menuItem, *item.url);

      if (index == -1) {
        [menu addItem:menuItem];
      } else {
        [menu insertItem:menuItem atIndex:index];
      }
    }
  }

  if (menuItem && item.items) {
    NSString* title = base::SysUTF8ToNSString(item.name);
    NSMenu* subMenu;

    // We want to keep the service sub menu of the original menu.
    bool isAppMenu = topLevel && item.id == IDC_CHROME_MENU;

    // This menu has issues on macOS 10.15+ (VB-58755)
    bool isWindowMenu = topLevel && item.id == IDC_WINDOW_MENU;

    // Special care for the bookmarks menu as we manage it two places.
    // Here and in bookmark_menu_bridge.mm. We never clear its content
    // and we replace menu items if they already exists.
    bool isBookmarkMenu = topLevel && item.id == IDC_BOOKMARKS_MENU;

    // Removing all items from the edit menu also removes special menu items
    // that are added by the OS. Avoid that.
    bool isEditMenu = topLevel && item.id == IDC_EDIT_MENU;
    long editMenuLength = 0;
    long appIndex = 0;

    if ([menuItem hasSubmenu]) {
      subMenu = [menuItem submenu];
      if (!isAppMenu && !isBookmarkMenu && !isEditMenu && !isWindowMenu) {
        [subMenu removeAllItems];
      }
      if (isEditMenu) {
        for (NSMenuItem* item in [subMenu itemArray]) {
          if ([item action] !=
                  NSSelectorFromString(@"orderFrontCharacterPalette:")) {
            [subMenu removeItem:item];
          }
        }
        [subMenu insertItem:[NSMenuItem separatorItem] atIndex:0];
        editMenuLength = [subMenu numberOfItems];
      }
      if (isAppMenu) {
        // Remove all but the Service menu and the 'Show All' entry. The service
        // menu is the only sub menu.
        for (NSMenuItem* item in [subMenu itemArray]) {
          if (!item.hasSubmenu &&
              [item action] !=
                  NSSelectorFromString(@"unhideAllApplications:")) {
            [subMenu removeItem:item];
          }
        }
      }
      if (isWindowMenu) {
        // Remove tab entries & kSeparatorTag, those are dynamically added when
        // there are changes.
        for (NSMenuItem* item in [subMenu itemArray]) {
          if (item.tag == IDC_VIV_ACTIVATE_TAB) {
            [subMenu removeItem:item];
          }
        }
      }
      subMenu.title = title;
    } else {
      subMenu = [[[NSMenu alloc] initWithTitle:title] autorelease];
      [menuItem setSubmenu:subMenu];
    }

    for (std::vector<menubar::MenuItem>::const_iterator it =
            item.items->begin();
        it != item.items->end(); ++it) {
      const menubar::MenuItem& child = *it;
      long index = -1;
      if (isAppMenu) {
        // Add all our menu items before the service menu until we are about
        // to add the 'hide vivaldi' entry. There we start to add after leaving
        // the service menu in front.
        // Next step one more to make room for the "Show All" entry that is to
        // be shown after "hide others".
        if (child.id == IDC_HIDE_APP) {
          NSMenuItem* menuItem = [NSMenuItem separatorItem];
          [subMenu insertItem:menuItem atIndex:appIndex + 1];
          appIndex += 2;
        }
        PopulateMenu(child, subMenu, false, appIndex, faviconLoader);
        if (child.id == IDC_VIV_HIDE_OTHERS) {
          appIndex ++;
        }
        appIndex ++;
        continue;
      }
      else if (isBookmarkMenu) {
        NSMenuItem* item = [subMenu itemWithTag:child.id];
        if (item) {
          // Just update the item (we may have a new shortcut etc).
          long index = [subMenu indexOfItem:item];
          [subMenu removeItemAtIndex:index];
          PopulateMenu(child, subMenu, false, index, faviconLoader);
          continue;
        } else {
          // The bookmark content can already be populated. Test for menu
          // size and insert item at given index if needed.
          if (child.index && subMenu.numberOfItems > *child.index) {
            index = *child.index;
          }
        }
      }
      else if (isEditMenu) {
        // insert our menuitems between the top of the menu and the
        // system menuitems that we left in the menu above.
        index = [subMenu numberOfItems] - editMenuLength;
        PopulateMenu(child, subMenu, false, index, faviconLoader);
        continue;
      }
      else if (isWindowMenu) {
        // Insert our open tab menuitems after the kSeparatorTag item,
        // before the list of open windows.
        NSMenuItem* item = [subMenu itemWithTag:kSeparatorTag];
        long index = [subMenu indexOfItem:item] + 1;
        PopulateMenu(child, subMenu, false, index, faviconLoader);
        continue;
      }
      PopulateMenu(child, subMenu, false, index, faviconLoader);
    }
  }
}

void UpdateMenuItem(const menubar::MenuItem& item, NSMenuItem* menuItem) {
  if (item.checked) {
    [menuItem setState:*item.checked ? NSOnState : NSOffState];
  }
  if (item.emphasized) {
    setMenuItemBold(menuItem, *item.emphasized.get() ? true : false);
  }
  bool visible = item.visible ? *item.visible : true;
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
    menubar::Mode mode) {

  NSMenu* mainMenu = [NSApp mainMenu];
  NSMenuItem* windowMenuItem = GetMenuItemByTag(mainMenu, IDC_WINDOW_MENU);

  if (!g_window_menu_delegate) {
    if ([windowMenuItem hasSubmenu]) {
      g_window_menu_delegate =
        [[MainMenuDelegate alloc] initWithProfile:profile
                                              tag:IDC_VIV_ACTIVATE_TAB];
      windowMenuItem.submenu.delegate = g_window_menu_delegate;
    }
  }

  if (mode == menubar::MODE_ALL) {
    // Full update. Remove any pending requests.
    [g_window_menu_delegate reset];

    FaviconLoaderMac* faviconLoader = [g_window_menu_delegate faviconLoader];
    for (const menubar::MenuItem& item : *items) {
      PopulateMenu(item, mainMenu, true, -1, faviconLoader);
    }
  } else if (mode == menubar::MODE_TABS) {
    [g_window_menu_delegate setMenuItems:items];
  } else if (mode == menubar::MODE_UPDATE) {
    // Update one or more items. Title is not changed.
    for (const menubar::MenuItem& item : *items) {
      NSMenuItem* menuItem = GetMenuItemByTag(mainMenu, item.id);
      if (menuItem)
        UpdateMenuItem(item, menuItem);
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
