// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/notes/note_home_shared_state.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"
#import "ios/ui/notes/cells/note_table_cell_title_editing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Minimium spacing between keyboard and the titleText when creating new folder,
// in points.
const CGFloat kKeyboardSpacingPt = 16.0;

}  // namespace

@implementation NoteHomeSharedState {
  std::set<const vivaldi::NoteNode*> _editNodes;
}

@synthesize addingNewFolder = _addingNewFolder;
@synthesize addingNewNote = _addingNewNote;
@synthesize notesModel = _notesModel;
@synthesize currentlyInEditMode = _currentlyInEditMode;
@synthesize currentlyShowingSearchResults = _currentlyShowingSearchResults;
@synthesize editingFolderCell = _editingFolderCell;
@synthesize editingFolderNode = _editingFolderNode;
@synthesize editingNoteCell = _editingNoteCell;
@synthesize editingNoteNode = _editingNoteNode;
@synthesize observer = _observer;
@synthesize promoVisible = _promoVisible;
@synthesize tableView = _tableView;
@synthesize tableViewDisplayedRootNode = _tableViewDisplayedRootNode;
@synthesize tableViewModel = _tableViewModel;

- (instancetype)initWithNotesModel:(vivaldi::NotesModel*)notesModel
                    displayedRootNode:
                        (const vivaldi::NoteNode*)displayedRootNode {
  if ((self = [super init])) {
    _notesModel = notesModel;
    _tableViewDisplayedRootNode = displayedRootNode;
  }
  return self;
}

- (void)setCurrentlyInEditMode:(BOOL)currentlyInEditMode {
  DCHECK(self.tableView);
  // If not in editing mode but the tableView's editing is ON, it means the
  // table is waiting for a swipe-to-delete confirmation.  In this case, we need
  // to close the confirmation by setting tableView.editing to NO.
  if (!_currentlyInEditMode && self.tableView.editing) {
    self.tableView.editing = NO;
  }
  [self.editingFolderCell stopEdit];
  [self.editingNoteCell stopEdit];
  _currentlyInEditMode = currentlyInEditMode;
  _editNodes.clear();
  [self.observer sharedStateDidClearEditNodes:self];
  [self.tableView setEditing:currentlyInEditMode animated:YES];
}

- (std::set<const vivaldi::NoteNode*>&)editNodes {
  return _editNodes;
}

+ (CGFloat)keyboardSpacingPt {
  return kKeyboardSpacingPt;
}

@end
