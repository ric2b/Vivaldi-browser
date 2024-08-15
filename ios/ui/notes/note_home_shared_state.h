// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_NOTES_NOTE_HOME_SHARED_STATE_H_
#define IOS_UI_NOTES_NOTE_HOME_SHARED_STATE_H_

#import <UIKit/UIKit.h>

#import <set>

#import "components/notes/notes_model.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"

@protocol NoteTableCellTitleEditing;
@class NoteHomeSharedState;
@class NoteTableCell;
@class TableViewModel;

namespace vivaldi {
class NoteModel;
class NoteNode;
}

typedef NS_ENUM(NSInteger, NoteHomeSectionIdentifier) {
  NoteHomeSectionIdentifierPromo = kSectionIdentifierEnumZero,
  NoteHomeSectionIdentifierNotes,
  NoteHomeSectionIdentifierMessages,
};

typedef NS_ENUM(NSInteger, NoteHomeItemType) {
  NoteHomeItemTypePromo = kItemTypeEnumZero,
  NoteHomeItemTypeNote,
  NoteHomeItemTypeMessage,
};

@protocol NoteHomeSharedStateObserver
// Called when the set of edit nodes is cleared.
- (void)sharedStateDidClearEditNodes:(NoteHomeSharedState*)sharedState;
@end

// NoteHomeSharedState is a data structure that contains a number of fields
// that were previously ivars of NoteTableView. They are moved to a separate
// data structure in order to ease moving code between files.
@interface NoteHomeSharedState : NSObject

// Models.

// The model backing the table view.
@property(nonatomic, strong) TableViewModel* tableViewModel;

// The model holding note data.
@property(nonatomic, readonly, assign) vivaldi::NotesModel* notesModel;

// Views.

// The UITableView to show notes.
@property(nonatomic, strong) UITableView* tableView;

// State variables.

// The NoteNode that is currently being displayed by the table view.  May be
// nil.
@property(nonatomic, assign)
    const vivaldi::NoteNode* tableViewDisplayedRootNode;

// If the table view is in edit mode.
@property(nonatomic, assign) BOOL currentlyInEditMode;

// If the table view showing search results.
@property(nonatomic, assign) BOOL currentlyShowingSearchResults;

// The set of nodes currently being edited.
@property(nonatomic, readonly, assign)
    std::set<const vivaldi::NoteNode*>& editNodes;

// If a new folder is being added currently.
@property(nonatomic, assign) BOOL addingNewFolder;
@property(nonatomic, assign) BOOL addingNewNote;

// The cell for the newly created folder while its name is being edited. Set
// to nil once the editing completes. Corresponds to |editingFolderNode|.
@property(nonatomic, weak)
    UITableViewCell<NoteTableCellTitleEditing>* editingFolderCell;

// The cell for the newly created folder while its name is being edited. Set
// to nil once the editing completes. Corresponds to |editingNoteNode|.
@property(nonatomic, weak)
    UITableViewCell<NoteTableCellTitleEditing>* editingNoteCell;

// The newly created folder node its name is being edited.
@property(nonatomic, assign) const vivaldi::NoteNode* editingFolderNode;

// The newly created note nodes name is being edited.
@property(nonatomic, assign) const vivaldi::NoteNode* editingNoteNode;

// True if the promo is visible.
@property(nonatomic, assign) BOOL promoVisible;

// This object can have at most one observer.
@property(nonatomic, weak) id<NoteHomeSharedStateObserver> observer;

// Constants

// Minimium spacing between keyboard and the titleText when creating new folder,
// in points.
+ (CGFloat)keyboardSpacingPt;

- (instancetype)initWithNotesModel:(vivaldi::NotesModel*)notesModel
                    displayedRootNode:
                        (const vivaldi::NoteNode*)displayedRootNode
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_UI_NOTES_NOTE_HOME_SHARED_STATE_H_
