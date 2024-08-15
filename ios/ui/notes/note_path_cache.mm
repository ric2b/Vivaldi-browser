// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/notes/note_path_cache.h"

#import "components/notes/note_node.h"
#import "components/notes/notes_model.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/ui/notes/note_utils_ios.h"
#import "prefs/vivaldi_pref_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using vivaldi::NotesModel;
using vivaldi::NoteNode;

namespace {
const int64_t kFolderNone = -1;
}  // namespace

@implementation NotePathCache

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterInt64Pref(vivaldiprefs::kVivaldiNoteCachedFolderId, kFolderNone);
  registry->RegisterIntegerPref(vivaldiprefs::kVivaldiNoteCachedTopMostRow, 0);
}

+ (void)cacheNoteTopMostRowWithPrefService:(PrefService*)prefService
                                      folderId:(int64_t)folderId
                                    topMostRow:(int)topMostRow {
  prefService->SetInt64(vivaldiprefs::kVivaldiNoteCachedFolderId, folderId);
  prefService->SetInteger(vivaldiprefs::kVivaldiNoteCachedTopMostRow, topMostRow);
}

+ (BOOL)getNoteTopMostRowCacheWithPrefService:(PrefService*)prefService
                                            model:
                                                (vivaldi::NotesModel*)model
                                         folderId:(int64_t*)folderId
                                       topMostRow:(int*)topMostRow {
  if (!folderId) return NO;

  *folderId = prefService->GetInt64(vivaldiprefs::kVivaldiNoteCachedFolderId);

  // If the cache was at root node, consider it as nothing was cached.
  if (*folderId == kFolderNone || *folderId == model->root_node()->id())
    return NO;

  // Create note Path.
  const NoteNode* note =
      note_utils_ios::FindFolderById(model, *folderId);
  // The note node is gone from model, maybe deleted remotely.
  if (!note)
    return NO;

  *topMostRow = prefService->GetInteger(vivaldiprefs::kVivaldiNoteCachedTopMostRow);
  return YES;
}

+ (void)clearNoteTopMostRowCacheWithPrefService:(PrefService*)prefService {
  prefService->SetInt64(vivaldiprefs::kVivaldiNoteCachedFolderId, kFolderNone);
}

@end
