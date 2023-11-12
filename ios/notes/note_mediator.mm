// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/notes/note_mediator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#include "base/strings/sys_string_conversions.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/url_with_title.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/notes/notes_factory.h"
#import "ios/notes/note_utils_ios.h"
#include "notes/note_node.h"
#include "notes/notes_model.h"
#include "notes/notes_model.h"
#include "notes/note_node.h"
#include "prefs/vivaldi_pref_names.h"
#include "ui/base/l10n/l10n_util.h"

#import "vivaldi/mobile_common/grit/vivaldi_mobile_common_native_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using vivaldi::NotesModel;
using vivaldi::NoteNode;

namespace {
const int64_t kLastUsedFolderNone = -1;
}  // namespace

@interface NoteMediator ()

// BrowserState for this mediator.
@property(nonatomic, assign) ChromeBrowserState* browserState;

@end

@implementation NoteMediator

@synthesize browserState = _browserState;

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterInt64Pref(vivaldiprefs::kVivaldiNoteFolderDefault,
                              kLastUsedFolderNone);
}

+ (const NoteNode*)folderForNewNotesInBrowserState:
    (ChromeBrowserState*)browserState {
  vivaldi::NotesModel* notes =
      vivaldi::NotesModelFactory::GetForBrowserState(browserState);
  const NoteNode* defaultFolder = notes->main_node();

  PrefService* prefs = browserState->GetPrefs();
  int64_t node_id = prefs->GetInt64(vivaldiprefs::kVivaldiNoteFolderDefault);
  if (node_id == kLastUsedFolderNone)
    node_id = defaultFolder->id();
  const NoteNode* result = notes->GetNoteNodeByID(node_id);

  if (result)
    return result;

  return defaultFolder;
}

+ (void)setFolderForNewNotes:(const NoteNode*)folder
                  inBrowserState:(ChromeBrowserState*)browserState {
  DCHECK(folder && folder->is_folder());
  browserState->GetPrefs()->SetInt64(vivaldiprefs::kVivaldiNoteFolderDefault,
                                     folder->id());
}

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState {
  self = [super init];
  if (self) {
    _browserState = browserState;
  }
  return self;
}

@end
