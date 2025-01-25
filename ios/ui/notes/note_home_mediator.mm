// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/notes/note_home_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/notes/notes_model.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_signin_promo_item.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/notes/cells/note_home_node_item.h"
#import "ios/ui/notes/note_home_consumer.h"
#import "ios/ui/notes/note_home_shared_state.h"
#import "ios/ui/notes/note_model_bridge_observer.h"
#import "ios/ui/notes/note_sorting_mode.h"
#import "ios/ui/notes/vivaldi_notes_pref.h"
#import "ui/base/l10n/l10n_util.h"


using vivaldi::NoteNode;
using l10n_util::GetNSString;

namespace {
// Maximum number of entries to fetch when searching.
const int kMaxNotesSearchResults = 50;
}  // namespace

namespace notes {
class NoteModelBridge;
}

@interface NoteHomeMediator () <NoteModelBridgeObserver,
                                    PrefObserverDelegate,
                                    SigninPresenter,
                                    SyncObserverModelBridge> {
  // Bridge to register for note changes.
  std::unique_ptr<notes::NoteModelBridge> _modelBridge;

  // Observer to keep track of the signin and syncing status.
  //std::unique_ptr<sync_vivaldi::SyncedNotesObserverBridge>
  //    _syncedNotesObserver;

  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;
}

// Shared state between Note home classes.
@property(nonatomic, strong) NoteHomeSharedState* sharedState;

// The profile for this mediator.
@property(nonatomic, assign) ProfileIOS* profile;

// Sync service.
@property(nonatomic, assign) syncer::SyncService* syncService;

@end

@implementation NoteHomeMediator
@synthesize profile = _profile;
@synthesize consumer = _consumer;
@synthesize sharedState = _sharedState;

- (instancetype)initWithSharedState:(NoteHomeSharedState*)sharedState
                            profile:(ProfileIOS*)profile {
  if ((self = [super init])) {
    _sharedState = sharedState;
    _profile = profile;
  }
  return self;
}

- (void)startMediating {
  DCHECK(self.consumer);
  DCHECK(self.sharedState);

  // Set up observers.
  _modelBridge = std::make_unique<notes::NoteModelBridge>(
      self, self.sharedState.notesModel);
  /*_syncedNotesObserver =
      std::make_unique<sync_vivaldi::SyncedNotesObserverBridge>(
          self, self.browserState);*/

  _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
  _prefChangeRegistrar->Init(self.profile->GetPrefs());
  _prefObserverBridge.reset(new PrefObserverBridge(self));

  // TODO_prefObserverBridge->ObserveChangesForPreference(
  //    prefs::kEditNotesEnabled, _prefChangeRegistrar.get());

  _syncService = SyncServiceFactory::GetForProfile(self.profile);

  [self computeNoteTableViewData];
}

- (void)disconnect {
  _modelBridge = nullptr;
  //_syncedNotesObserver = nullptr;
  self.profile = nullptr;
  self.consumer = nil;
  self.sharedState = nil;
  _prefChangeRegistrar.reset();
  _prefObserverBridge.reset();
}

#pragma mark - Initial Model Setup

// Computes the notes table view based on the current root node.
- (void)computeNoteTableViewData {
  [self deleteAllItemsOrAddSectionWithIdentifier:
            NoteHomeSectionIdentifierNotes];
  [self deleteAllItemsOrAddSectionWithIdentifier:
            NoteHomeSectionIdentifierMessages];

  // Regenerate the list of all notes.
  if (!self.sharedState.notesModel->loaded() ||
      !self.sharedState.tableViewDisplayedRootNode) {
    [self updateTableViewBackground];
    return;
  }

  if (self.sharedState.tableViewDisplayedRootNode ==
      self.sharedState.notesModel->root_node()) {
    [self generateTableViewDataForRootNode];
    [self updateTableViewBackground];
    return;
  }
  [self generateSortedTableViewData];
  [self updateTableViewBackground];
}

// Generate the table view data when the current root node is a child node.
- (void)generateTableViewData {
  if (!self.sharedState.tableViewDisplayedRootNode) {
    return;
  }
  // Add all notes and folders of the current root node to the table.
  for (const auto& child :
       self.sharedState.tableViewDisplayedRootNode->children()) {
    NoteHomeNodeItem* nodeItem =
        [[NoteHomeNodeItem alloc] initWithType:NoteHomeItemTypeNote
                                      noteNode:child.get()];
    [self.sharedState.tableViewModel
                        addItem:nodeItem
        toSectionWithIdentifier:NoteHomeSectionIdentifierNotes];
  }
}

/// Returns current sorting mode
- (NotesSortingMode)currentSortingMode {
  return [VivaldiNotesPrefs getNotesSortingMode];
}

/// Returns current sorting order
- (NotesSortingOrder)currentSortingOrder {
  return [VivaldiNotesPrefs getNotesSortingOrder];
}

// sort the data for the table view
- (void)generateSortedTableViewData {
  if (!self.sharedState.tableViewDisplayedRootNode) {
    return;
  }

  NSMutableArray<NoteHomeNodeItem*> *nodeItems = [NSMutableArray array];

  // Add all notes and folders of the current root node to the table.
  for (const auto& child :
     self.sharedState.tableViewDisplayedRootNode->children()) {
    NoteHomeNodeItem* nodeItem =
      [[NoteHomeNodeItem alloc]
        initWithType:NoteHomeItemTypeNote
            noteNode:child.get()];
    [nodeItems addObject:nodeItem];
  }

  // Sort the nodeItems array based on the current sorting mode
  switch (self.currentSortingMode) {
    case NotesSortingModeManual:
      // Doesn't need to apply any sorting in manual sort mode
      break;
    case NotesSortingModeTitle:
      [nodeItems sortUsingComparator:
          ^NSComparisonResult(NoteHomeNodeItem* a, NoteHomeNodeItem* b) {
        return [a.noteTitle compare:b.noteTitle];
      }];
      break;
    case NotesSortingModeDateCreated:
      [nodeItems sortUsingComparator:
          ^NSComparisonResult(NoteHomeNodeItem* a, NoteHomeNodeItem* b) {
        return [a.createdAt compare:b.createdAt];
      }];
      break;
    case NotesSortingModeDateEdited:
      [nodeItems sortUsingComparator:
          ^NSComparisonResult(NoteHomeNodeItem* a, NoteHomeNodeItem* b) {
        return [b.lastModified compare:a.lastModified];
      }];
      break;
    case NotesSortingModeByKind:
      [nodeItems sortUsingComparator:
          ^NSComparisonResult(NoteHomeNodeItem* a, NoteHomeNodeItem* b) {
        return [self compare:a.isFolder
                      second:b.isFolder
                foldersFirst:YES];
      }];
      break;
  }

  // If the current sorting order is descending
  // Reverse the array & check it is not sort by NotesSortingModeManual
  if (self.currentSortingOrder == NotesSortingOrderDescending &&
      self.currentSortingMode != NotesSortingModeManual) {
    nodeItems = [[[nodeItems reverseObjectEnumerator] allObjects] mutableCopy];
  }

  // Add all the nodes to tableViewModel
  for (NoteHomeNodeItem* nodeItem in nodeItems) {
    [self.sharedState.tableViewModel addItem:nodeItem
                     toSectionWithIdentifier:NoteHomeSectionIdentifierNotes];
  }
}

/// Returns sorted result from two provided NSString keys.
- (NSComparisonResult)compare:(NSString*)first
             second:(NSString*)second {
  return [VivaldiGlobalHelpers compare:first second:second];
}

/// Returns sorted result from two provided BOOL keys, and sorting order.
- (NSComparisonResult)compare:(BOOL)first
                       second:(BOOL)second
                 foldersFirst:(BOOL)foldersFirst {
  return [VivaldiGlobalHelpers compare:first
                                second:second
                          foldersFirst:foldersFirst];
}

// Generate the table view data when the current root node is the outermost
// root.
- (void)generateTableViewDataForRootNode {

  if (![self hasNotesOrFolders]) {
    return;
  }

  // Add root "Notes" to the table.
  const NoteNode* rootNode =
      self.sharedState.notesModel->main_node();
  NoteHomeNodeItem* mobileItem =
      [[NoteHomeNodeItem alloc] initWithType:NoteHomeItemTypeNote
                                    noteNode:rootNode];
  [self.sharedState.tableViewModel
                      addItem:mobileItem
      toSectionWithIdentifier:NoteHomeSectionIdentifierNotes];

  // Add "Trash" notes to the table.
  const NoteNode* trashNode =
      self.sharedState.notesModel->trash_node();
  NoteHomeNodeItem* trashItem =
      [[NoteHomeNodeItem alloc] initWithType:NoteHomeItemTypeNote
                                    noteNode:trashNode];
  trashItem.shouldShowTrashIcon = YES;
  [self.sharedState.tableViewModel
                      addItem:trashItem
      toSectionWithIdentifier:NoteHomeSectionIdentifierNotes];
}

- (void)computeNoteTableViewDataMatching:(NSString*)searchText
                  orShowMessageWhenNoResults:(NSString*)noResults {
  [self deleteAllItemsOrAddSectionWithIdentifier:
            NoteHomeSectionIdentifierNotes];
  [self deleteAllItemsOrAddSectionWithIdentifier:
            NoteHomeSectionIdentifierMessages];

  std::vector<const NoteNode*> nodes;
  self.sharedState.notesModel->GetNotesMatching(
        base::SysNSStringToUTF16(searchText),
        kMaxNotesSearchResults,
        nodes);
  int count = 0;
  for (const NoteNode* node : nodes) {
    NoteHomeNodeItem* nodeItem =
        [[NoteHomeNodeItem alloc] initWithType:NoteHomeItemTypeNote
                                      noteNode:node];
    [self.sharedState.tableViewModel
                        addItem:nodeItem
        toSectionWithIdentifier:NoteHomeSectionIdentifierNotes];
    count++;
  }

  if (count == 0) {
    TableViewTextItem* item =
        [[TableViewTextItem alloc] initWithType:NoteHomeItemTypeMessage];
    item.textAlignment = NSTextAlignmentLeft;
    item.textColor = [UIColor colorNamed:kTextPrimaryColor];
    item.text = noResults;
    [self.sharedState.tableViewModel
                        addItem:item
        toSectionWithIdentifier:NoteHomeSectionIdentifierMessages];
    return;
  }

  [self updateTableViewBackground];
}

- (void)updateTableViewBackground {
  // If the current root node is the outermost root, check if we need to show
  // the spinner backgound.  Otherwise, check if we need to show the empty
  // background.
  if (self.sharedState.tableViewDisplayedRootNode ==
      self.sharedState.notesModel->root_node()) {
    /* TODO if (self.sharedState.notesModel->HasNoUserCreatedNotesOrFolders()
     &&   _syncedNotesObserver->IsPerformingInitialSync()) {
      [self.consumer
          updateTableViewBackgroundStyle:NoteHomeBackgroundStyleLoading];
    } else*/ if (![self hasNotesOrFolders]) {
      [self.consumer
          updateTableViewBackgroundStyle:NoteHomeBackgroundStyleEmpty];
    } else {
      [self.consumer
          updateTableViewBackgroundStyle:NoteHomeBackgroundStyleDefault];
    }
    return;
  }

  if (![self hasNotesOrFolders] &&
      !self.sharedState.currentlyShowingSearchResults) {
    [self.consumer
        updateTableViewBackgroundStyle:NoteHomeBackgroundStyleEmpty];
  } else {
    [self.consumer
        updateTableViewBackgroundStyle:NoteHomeBackgroundStyleDefault];
  }
}

#pragma mark - NoteModelBridgeObserver Callbacks

// NoteModelBridgeObserver Callbacks
// Instances of this class automatically observe the note model.
// The note model has loaded.
- (void)noteModelLoaded {
  [self.consumer refreshContents];
}

// The node has changed, but not its children.
- (void)noteNodeChanged:(const NoteNode*)noteNode {
  // The root folder changed. Do nothing.
  if (noteNode == self.sharedState.tableViewDisplayedRootNode) {
    return;
  }

  // A specific cell changed. Reload, if currently shown.
  if ([self itemForNode:noteNode] != nil) {
    [self.consumer refreshContents];
  }
}

// The node has not changed, but its children have.
- (void)noteNodeChildrenChanged:(const NoteNode*)noteNode {
  // In search mode, we want to refresh any changes (like undo).
  if (self.sharedState.currentlyShowingSearchResults) {
    [self.consumer refreshContents];
  }
  // The current root folder's children changed. Reload everything.
  // (When adding new folder, table is already been updated. So no need to
  // reload here.)
  // Or the current roots grand children changed
  if ((noteNode == self.sharedState.tableViewDisplayedRootNode ||
      noteNode->parent() == self.sharedState.tableViewDisplayedRootNode
      ) &&
      !self.sharedState.addingNewFolder && !self.sharedState.addingNewNote) {
    [self.consumer refreshContents];
    return;
  }
}

// The node has moved to a new parent folder.
// This might mean the count needs updating
- (void)noteNode:(const NoteNode*)noteNode
     movedFromParent:(const NoteNode*)oldParent
            toParent:(const NoteNode*)newParent {
  if (oldParent == self.sharedState.tableViewDisplayedRootNode ||
      newParent == self.sharedState.tableViewDisplayedRootNode ||
      oldParent->parent() == self.sharedState.tableViewDisplayedRootNode ||
      newParent->parent() == self.sharedState.tableViewDisplayedRootNode
      ) {
      // A folder was added or removed from the current root folder or its child.
     [self.consumer refreshContents];
  }
}

// |node| was deleted from |folder|.
- (void)noteNodeDeleted:(const NoteNode*)node
                 fromFolder:(const NoteNode*)folder {
  if (self.sharedState.currentlyShowingSearchResults) {
    [self.consumer refreshContents];
  } else if (self.sharedState.tableViewDisplayedRootNode == node) {
    self.sharedState.tableViewDisplayedRootNode = NULL;
    [self.consumer refreshContents];
  }
}

// All non-permanent nodes have been removed.
- (void)noteModelRemovedAllNodes {
  // TODO(crbug.com/695749) Check if this case is applicable in the new UI.
}

- (NoteHomeNodeItem*)itemForNode:
    (const vivaldi::NoteNode*)noteNode {
  NSArray<TableViewItem*>* items = [self.sharedState.tableViewModel
      itemsInSectionWithIdentifier:NoteHomeSectionIdentifierNotes];
  for (TableViewItem* item in items) {
    if (item.type == NoteHomeItemTypeNote) {
      NoteHomeNodeItem* nodeItem =
          base::apple::ObjCCastStrict<NoteHomeNodeItem>(item);
      if (nodeItem.noteNode == noteNode) {
        return nodeItem;
      }
    }
  }
  return nil;
}

#pragma mark - SigninPresenter

- (void)showSignin:(ShowSigninCommand*)command {
  // Proxy this call along to the consumer.
  [self.consumer showSignin:command];
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  // Permanent nodes ("Notes Bar", "Other Notes") at the root node might
  // be added after syncing.  So we need to refresh here.
  if (self.sharedState.tableViewDisplayedRootNode ==
          self.sharedState.notesModel->root_node() ||
      self.isSyncDisabledByAdministrator) {
    [self.consumer refreshContents];
    return;
  }
  [self updateTableViewBackground];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  // Editing capability may need to be updated on the notes UI.
  // Or managed notes contents may need to be updated.
  // TODO if (preferenceName == prefs::kEditNotesEnabled) {
    [self.consumer refreshContents];
  //}
}

#pragma mark - Private Helpers

- (BOOL)hasNotesOrFolders {
  if (self.sharedState.tableViewDisplayedRootNode ==
      self.sharedState.notesModel->root_node()) {
    // The root node always has its permanent nodes. We will consider it has
    // folders as we will display empty  folders on the root page.
    return YES;
  }
  return self.sharedState.tableViewDisplayedRootNode &&
         !self.sharedState.tableViewDisplayedRootNode->children().empty();
}

// Delete all items for the given |sectionIdentifier| section, or create it
// if it doesn't exist, hence ensuring the section exists and is empty.
- (void)deleteAllItemsOrAddSectionWithIdentifier:(NSInteger)sectionIdentifier {
  if ([self.sharedState.tableViewModel
          hasSectionForSectionIdentifier:sectionIdentifier]) {
    [self.sharedState.tableViewModel
        deleteAllItemsFromSectionWithIdentifier:sectionIdentifier];
  } else {
    [self.sharedState.tableViewModel
        addSectionWithIdentifier:sectionIdentifier];
  }
}

// Returns YES if the user cannot turn on sync for enterprise policy reasons.
- (BOOL)isSyncDisabledByAdministrator {
  DCHECK(self.syncService);
    bool syncDisabledPolicy = YES; //self.syncService->GetDisableReasons().Has(
    //  syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
  return syncDisabledPolicy ;
}
@end
