// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/notes/note_path_cache.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/notes/note_utils_ios.h"
#include "notes/notes_model.h"
#include "notes/note_node.h"
#include "prefs/vivaldi_pref_names.h"

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
  registry->RegisterInt64Pref(vivaldiprefs::kIosNoteCachedFolderId, kFolderNone);
  registry->RegisterIntegerPref(vivaldiprefs::kIosNoteCachedTopMostRow, 0);
}

+ (void)cacheNoteTopMostRowWithPrefService:(PrefService*)prefService
                                      folderId:(int64_t)folderId
                                    topMostRow:(int)topMostRow {
  prefService->SetInt64(vivaldiprefs::kIosNoteCachedFolderId, folderId);
  prefService->SetInteger(vivaldiprefs::kIosNoteCachedTopMostRow, topMostRow);
}

+ (BOOL)getNoteTopMostRowCacheWithPrefService:(PrefService*)prefService
                                            model:
                                                (vivaldi::NotesModel*)model
                                         folderId:(int64_t*)folderId
                                       topMostRow:(int*)topMostRow {
  *folderId = prefService->GetInt64(vivaldiprefs::kIosNoteCachedFolderId);

  // If the cache was at root node, consider it as nothing was cached.
  if (*folderId == kFolderNone || *folderId == model->root_node()->id())
    return NO;

  // Create note Path.
  const NoteNode* note =
      note_utils_ios::FindFolderById(model, *folderId);
  // The note node is gone from model, maybe deleted remotely.
  if (!note)
    return NO;

  *topMostRow = prefService->GetInteger(vivaldiprefs::kIosNoteCachedTopMostRow);
  return YES;
}

+ (void)clearNoteTopMostRowCacheWithPrefService:(PrefService*)prefService {
  prefService->SetInt64(vivaldiprefs::kIosNoteCachedFolderId, kFolderNone);
}

@end
