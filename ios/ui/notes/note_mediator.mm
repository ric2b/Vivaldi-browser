// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/notes/note_mediator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/strings/sys_string_conversions.h"
#import "components/notes/note_node.h"
#import "components/notes/note_node.h"
#import "components/notes/notes_model.h"
#import "components/notes/notes_model.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/notes/notes_factory.h"
#import "ios/ui/notes/note_utils_ios.h"
#import "prefs/vivaldi_pref_names.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using vivaldi::NotesModel;
using vivaldi::NoteNode;

namespace {
const int64_t kLastUsedFolderNone = -1;
}  // namespace

@interface NoteMediator ()

// Profile for this mediator.
@property(nonatomic, assign) ProfileIOS* profile;

@end

@implementation NoteMediator

@synthesize profile = _profile;

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterInt64Pref(vivaldiprefs::kVivaldiNoteFolderDefault,
                              kLastUsedFolderNone);
}

+ (const NoteNode*)folderForNewNotesInProfile:(ProfileIOS*)profile {
  vivaldi::NotesModel* notes =
      vivaldi::NotesModelFactory::GetForProfile(profile);
  const NoteNode* defaultFolder = notes->main_node();

  PrefService* prefs = profile->GetPrefs();
  int64_t node_id = prefs->GetInt64(vivaldiprefs::kVivaldiNoteFolderDefault);
  if (node_id == kLastUsedFolderNone)
    node_id = defaultFolder->id();
  const NoteNode* result = notes->GetNoteNodeByID(node_id);

  if (result)
    return result;

  return defaultFolder;
}

+ (void)setFolderForNewNotes:(const NoteNode*)folder
                   inProfile:(ProfileIOS*)profile {
  DCHECK(folder && folder->is_folder());
  profile->GetPrefs()->SetInt64(vivaldiprefs::kVivaldiNoteFolderDefault,
                                folder->id());
}

- (instancetype)initWithProfile:(ProfileIOS*)profile {
  self = [super init];
  if (self) {
    _profile = profile;
  }
  return self;
}

@end
