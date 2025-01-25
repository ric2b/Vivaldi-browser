// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_mediator.h"

#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import <memory>

#import "base/debug/dump_without_crashing.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/scoped_multi_source_observation.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/commerce/model/shopping_persisted_data_tab_helper.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/drag_and_drop/model/drag_item_util.h"
#import "ios/chrome/browser/iph_for_new_chrome_user/model/tab_based_iph_browser_agent.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/reading_list/model/reading_list_browser_agent.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/reading_list_add_command.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_toolbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "ios/chrome/browser/snapshots/model/model_swift.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id_wrapper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_storage_wrapper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/tabs_search/model/tabs_search_service.h"
#import "ios/chrome/browser/tabs_search/model/tabs_search_service_factory.h"
#import "ios/chrome/browser/ui/menu/action_factory.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_mediator_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/selected_grid_items.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_idle_status_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_configuration.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_main_tab_grid_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "net/base/apple/url_conversions.h"
#import "ui/gfx/image/image.h"

using PinnedState = WebStateSearchCriteria::PinnedState;

namespace {

void LogPriceDropMetrics(web::WebState* web_state) {
  ShoppingPersistedDataTabHelper* shopping_helper =
      ShoppingPersistedDataTabHelper::FromWebState(web_state);
  if (!shopping_helper) {
    return;
  }
  const ShoppingPersistedDataTabHelper::PriceDrop* price_drop =
      shopping_helper->GetPriceDrop();
  BOOL has_price_drop =
      price_drop && price_drop->current_price && price_drop->previous_price;
  base::RecordAction(base::UserMetricsAction(
      base::StringPrintf("Commerce.TabGridSwitched.%s",
                         has_price_drop ? "HasPriceDrop" : "NoPriceDrop")
          .c_str()));
}

// Returns the Browser with `identifier` in its WebStateList. Returns `nullptr`
// if not found.
Browser* GetBrowserForNonPinnedTabWithId(BrowserList* browser_list,
                                         web::WebStateID identifier,
                                         bool is_otr_tab) {
  const BrowserList::BrowserType browser_types =
      is_otr_tab ? BrowserList::BrowserType::kIncognito
                 : BrowserList::BrowserType::kRegularAndInactive;
  std::set<Browser*> browsers = browser_list->BrowsersOfType(browser_types);
  for (Browser* browser : browsers) {
    WebStateList* web_state_list = browser->GetWebStateList();
    int index = GetWebStateIndex(web_state_list,
                                 WebStateSearchCriteria{
                                     .identifier = identifier,
                                     .pinned_state = PinnedState::kNonPinned,
                                 });
    if (index != WebStateList::kInvalidIndex) {
      return browser;
    }
  }
  return nullptr;
}

}  // namespace

@interface BaseGridMediator () <CRWWebStateObserver, SnapshotStorageObserver>
// The browser state from the browser.
@property(nonatomic, readonly) ChromeBrowserState* browserState;

@end

@implementation BaseGridMediator {
  // Observers for WebStateList.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<
      base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>>
      _scopedWebStateListObservation;
  // Observer for WebStates.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  std::unique_ptr<
      base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>>
      _scopedWebStateObservation;

  // ItemID of the dragged tab. Used to check if the dropped tab is from the
  // same Chrome window.
  web::WebStateID _dragItemID;
  base::WeakPtr<Browser> _browser;

  // Current mode.
  TabGridMode _currentMode;

  // Items selected for editing.
  SelectedGridItems* _selectedEditingItems;
}

- (instancetype)init {
  if (self = [super init]) {
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
    _scopedWebStateListObservation = std::make_unique<
        base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>>(
        _webStateListObserverBridge.get());
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _scopedWebStateObservation =
        std::make_unique<base::ScopedMultiSourceObservation<
            web::WebState, web::WebStateObserver>>(
            _webStateObserverBridge.get());
    _currentMode = TabGridModeNormal;
  }
  return self;
}

#pragma mark - Public properties

- (Browser*)browser {
  return _browser.get();
}

- (TabGridMode)currentMode {
  return _currentMode;
}

- (void)setBrowser:(Browser*)browser {
  [self.snapshotStorage removeObserver:self];
  _scopedWebStateListObservation->RemoveAllObservations();
  _scopedWebStateObservation->RemoveAllObservations();

  _browser.reset();
  if (browser) {
    _browser = browser->AsWeakPtr();
  }

  _webStateList = browser ? browser->GetWebStateList() : nullptr;
  _browserState = browser ? browser->GetBrowserState() : nullptr;

  [self.snapshotStorage addObserver:self];

  if (_webStateList) {
    _scopedWebStateListObservation->AddObservation(_webStateList);
    [self addWebStateObservations];
    _selectedEditingItems =
        [[SelectedGridItems alloc] initWithWebStateList:_webStateList];

    if (self.webStateList->count() > 0) {
      [self populateConsumerItems];
    }
  }
}

- (void)setConsumer:(id<TabCollectionConsumer>)consumer {
  _consumer = consumer;
  [self resetToAllItems];
}

- (void)setCurrentMode:(TabGridMode)mode {
  if (_currentMode != mode && (_currentMode == TabGridModeSelection ||
                               _currentMode == TabGridModeSearch)) {
    // Clear selections.
    [_selectedEditingItems removeAllItems];
  }
  _currentMode = mode;
  [self configureToolbarsButtons];
  [self.gridConsumer setPageMode:_currentMode];
}

#pragma mark - Subclassing

- (void)disconnect {
  _browser.reset();
  _browserState = nil;
  _consumer = nil;
  _delegate = nil;
  _toolbarsMutator = nil;
  _containedGridToolbarsProvider = nil;
  _toolbarTabGridDelegate = nil;
  _gridConsumer = nil;
  _tabPresentationDelegate = nil;

  _scopedWebStateListObservation->RemoveAllObservations();
  _scopedWebStateObservation->RemoveAllObservations();
  _scopedWebStateObservation.reset();
  _scopedWebStateListObservation.reset();

  _webStateObserverBridge.reset();
  _webStateList->RemoveObserver(_webStateListObserverBridge.get());
  _webStateListObserverBridge.reset();
  _webStateList = nil;
}

- (void)configureToolbarsButtons {
  NOTREACHED_NORETURN() << "Should be implemented in a subclass.";
}

- (void)configureButtonsInSelectionMode:
    (TabGridToolbarsConfiguration*)configuration {
  NSUInteger selectedItemsCount = _selectedEditingItems.tabsCount;
  NSUInteger selectedShareableItemsCount =
      _selectedEditingItems.sharableTabsCount;

  BOOL allItemsSelected =
      static_cast<int>(selectedItemsCount) ==
      (self.webStateList->count() - self.webStateList->pinned_tabs_count());

  configuration.selectAllButton = !allItemsSelected;
  configuration.deselectAllButton = allItemsSelected;
  configuration.doneButton = YES;
  configuration.closeSelectedTabsButton = selectedItemsCount > 0;
  configuration.shareButton = selectedShareableItemsCount > 0;
  if (IsTabGroupInGridEnabled()) {
    configuration.addToButton = selectedItemsCount > 0;
  } else {
    configuration.addToButton = selectedShareableItemsCount > 0;
  }
  configuration.selectedItemsCount = selectedItemsCount;

  configuration.addToButtonMenu =
      [UIMenu menuWithChildren:[self addToButtonMenuElements]];
}

- (void)displayActiveTab {
  NOTREACHED_NORETURN() << "Should be implemented in a subclass.";
}

- (void)populateConsumerItems {
  if (!self.webStateList) {
    return;
  }
  [self.consumer populateItems:CreateItems(self.webStateList)
        selectedItemIdentifier:[self activeIdentifier]];
}

- (GridItemIdentifier*)activeIdentifier {
  WebStateList* webStateList = self.webStateList;
  if (!webStateList) {
    return nil;
  }

  int webStateIndex = webStateList->active_index();
  if (webStateIndex == WebStateList::kInvalidIndex) {
    return nil;
  }

  if (IsTabGroupInGridEnabled()) {
    const TabGroup* group = webStateList->GetGroupOfWebStateAt(webStateIndex);
    if (group) {
      return [GridItemIdentifier groupIdentifier:group
                                withWebStateList:webStateList];
    }
  }

  return [GridItemIdentifier
      tabIdentifier:webStateList->GetWebStateAt(webStateIndex)];
}

- (void)addWebStateObservations {
  int firstIndex =
      IsPinnedTabsEnabled() ? self.webStateList->pinned_tabs_count() : 0;
  for (int i = firstIndex; i < self.webStateList->count(); i++) {
    web::WebState* webState = self.webStateList->GetWebStateAt(i);
    [self addObservationForWebState:webState];
  }
}

- (void)addObservationForWebState:(web::WebState*)webState {
  _scopedWebStateObservation->AddObservation(webState);
}

- (void)removeObservationForWebState:(web::WebState*)webState {
  _scopedWebStateObservation->RemoveObservation(webState);
}

- (void)insertNewWebStateAtGridIndex:(int)index withURL:(const GURL&)newTabURL {
  // The incognito mediator's Browser is briefly set to nil after the last
  // incognito tab is closed.  This occurs because the incognito BrowserState
  // needs to be destroyed to correctly clear incognito browsing data.  Don't
  // attempt to create a new WebState with a nil BrowserState.
  if (!self.browser) {
    return;
  }

  // There are some circumstances where a new tab insertion can be erroniously
  // triggered while another web state list mutation is happening. To ensure
  // those bugs don't become crashes, check that the web state list is OK to
  // mutate.
  if (self.webStateList->IsMutating()) {
    // Shouldn't have happened!
    DCHECK(false) << "Reentrant web state insertion!";
    return;
  }

  DCHECK(self.browserState);
  web::WebState::CreateParams params(self.browserState);
  std::unique_ptr<web::WebState> webState = web::WebState::Create(params);

  int webStateListIndex =
      WebStateIndexFromGridDropItemIndex(self.webStateList, index);
  webStateListIndex =
      std::clamp(webStateListIndex, 0, self.webStateList->count());

  web::NavigationManager::WebLoadParams loadParams(newTabURL);
  loadParams.transition_type = ui::PAGE_TRANSITION_TYPED;
  webState->GetNavigationManager()->LoadURLWithParams(loadParams);

  self.webStateList->InsertWebState(
      std::move(webState),
      WebStateList::InsertionParams::AtIndex(webStateListIndex).Activate());
}

- (void)insertItem:(GridItemIdentifier*)item
    beforeWebStateIndex:(int)nextWebStateIndex {
  WebStateList* webStateList = self.webStateList;
  GridItemIdentifier* nextItemIdentifier;
  if (webStateList->ContainsIndex(nextWebStateIndex)) {
    const TabGroup* group =
        webStateList->GetGroupOfWebStateAt(nextWebStateIndex);
    if (group) {
      nextItemIdentifier = [GridItemIdentifier groupIdentifier:group
                                              withWebStateList:webStateList];
    } else {
      nextItemIdentifier = [GridItemIdentifier
          tabIdentifier:self.webStateList->GetWebStateAt(nextWebStateIndex)];
    }
  }
  [self.consumer insertItem:item
                beforeItemID:nextItemIdentifier
      selectedItemIdentifier:[self activeIdentifier]];
}

- (void)moveItem:(GridItemIdentifier*)item
    beforeWebStateIndex:(int)nextWebStateIndex {
  GridItemIdentifier* nextItem = nil;
  if (self.webStateList->ContainsIndex(nextWebStateIndex)) {
    const TabGroup* group =
        self.webStateList->GetGroupOfWebStateAt(nextWebStateIndex);
    if (group) {
      nextItem = [GridItemIdentifier groupIdentifier:group
                                    withWebStateList:self.webStateList];
    } else {
      nextItem = [GridItemIdentifier
          tabIdentifier:self.webStateList->GetWebStateAt(nextWebStateIndex)];
    }
  }

  [self.consumer moveItem:item beforeItem:nextItem];
}

- (void)updateConsumerItemForWebState:(web::WebState*)webState {
  WebStateList* webStateList = self.webStateList;
  int index = webStateList->GetIndexOfWebState(webState);
  const TabGroup* group = nullptr;
  if (webStateList->ContainsIndex(index)) {
    group = webStateList->GetGroupOfWebStateAt(index);
  }
  GridItemIdentifier* item;
  if (group) {
    item = [GridItemIdentifier groupIdentifier:group
                              withWebStateList:webStateList];
  } else {
    item = [GridItemIdentifier tabIdentifier:webState];
  }
  [self.consumer replaceItem:item withReplacementItem:item];
}

- (void)closeTabGroup:(const TabGroup*)group andDeleteGroup:(BOOL)deleteGroup {
  [self.tabGridIdleStatusHandler
      tabGridDidPerformAction:TabGridActionType::kInPageAction];

  WebStateList* groupWebStateList = [self groupWebStateList:group];
  if (!groupWebStateList) {
    // The group has already been removed.
    return;
  }

  if (groupWebStateList != _webStateList) {
    // `group` is not in the set of groups of the `_webStateList`, so `group`
    // should be a search result from a different window. Since this item is not
    // from the current browser, no UI updates will be sent to the current grid.
    // Notify the current grid consumer about the change.
    CHECK(self.currentMode == TabGridModeSearch, base::NotFatalUntil::M130);
    GridItemIdentifier* identifierToRemove =
        [GridItemIdentifier groupIdentifier:group
                           withWebStateList:groupWebStateList];
    [self.consumer removeItemWithIdentifier:identifierToRemove
                     selectedItemIdentifier:nil];
  }

  if (IsTabGroupSyncEnabled() && !deleteGroup) {
    // TODO(crbug.com/329627077): Add a mechanism to show it only once.
    [self.tabGridToolbarHandler showSavedTabGroupIPH];
    tab_groups::TabGroupSyncService* syncService =
        tab_groups::TabGroupSyncServiceFactory::GetForBrowserState(
            self.browser->GetBrowserState());
    tab_groups::utils::CloseTabGroupLocally(group, groupWebStateList,
                                            syncService);
  } else {
    // Using `CloseAllWebStatesInGroup` will result in calling the web state
    // list observers which will take care of updating the consumer.
    CloseAllWebStatesInGroup(*groupWebStateList, group,
                             WebStateList::CLOSE_USER_ACTION);
  }
}

- (void)ungroupTabGroup:(const TabGroup*)group {
  [self.tabGridIdleStatusHandler
      tabGridDidPerformAction:TabGridActionType::kInPageAction];

  WebStateList* groupWebStateList = [self groupWebStateList:group];
  if (!groupWebStateList) {
    // The group has already been removed.
    return;
  }

  if (groupWebStateList != _webStateList) {
    // `group` is not in the set of groups of the `_webStateList`, so `group`
    // should be a search result from a different window. Since this item is not
    // from the current browser, no UI updates will be sent to the current grid.
    // Notify the current grid consumer about the change.
    CHECK(self.currentMode == TabGridModeSearch, base::NotFatalUntil::M130);
    GridItemIdentifier* identifierToRemove =
        [GridItemIdentifier groupIdentifier:group
                           withWebStateList:groupWebStateList];
    [self.consumer removeItemWithIdentifier:identifierToRemove
                     selectedItemIdentifier:nil];
  }

  groupWebStateList->DeleteGroup(group);
}

#pragma mark - WebStateListObserving

- (void)willChangeWebStateList:(WebStateList*)webStateList
                        change:(const WebStateListChangeDetach&)detachChange
                        status:(const WebStateListStatus&)status {
  DCHECK_EQ(_webStateList, webStateList);
  if (webStateList->IsBatchInProgress()) {
    return;
  }

  // When the deleted tab is showing as an item (i.e. when it's not
  // grouped or shown as a search result), remove it from the grid.
  if (!detachChange.group() || self.currentMode == TabGridModeSearch) {
    // Get the identifier to remove.
    web::WebState* detachedWebState = detachChange.detached_web_state();
    GridItemIdentifier* identifierToRemove =
        [GridItemIdentifier tabIdentifier:detachedWebState];

    // If the WebState is pinned and it is not in the consumer's items list,
    // consumer will filter it out in the method's implementation.
    [self.consumer removeItemWithIdentifier:identifierToRemove
                     selectedItemIdentifier:[self activeIdentifier]];
    [self removeFromSelectionItemID:identifierToRemove];
  }

  // The pinned WebState could be detached only in case it was displayed in
  // the Tab Search and was closed from the context menu. In such a case
  // there were no observation added for it. Therefore, there is no need to
  // remove one.
  if (![self isPinnedWebState:detachChange.detached_from_index()]) {
    [self removeObservationForWebState:detachChange.detached_web_state()];
  }
}

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  DCHECK_EQ(_webStateList, webStateList);
  if (webStateList->IsBatchInProgress()) {
    return;
  }

  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly: {
      const WebStateListChangeStatusOnly& selectionOnlyChange =
          change.As<WebStateListChangeStatusOnly>();
      if (selectionOnlyChange.pinned_state_changed()) {
        [self
            changePinnedStateForWebState:selectionOnlyChange.web_state()
                                 atIndex:selectionOnlyChange.index()
                            updatedGroup:selectionOnlyChange.old_group()
                                             ? selectionOnlyChange.old_group()
                                             : selectionOnlyChange.new_group()];
        break;
      }
      const TabGroup* oldGroup = selectionOnlyChange.old_group();
      const TabGroup* newGroup = selectionOnlyChange.new_group();

      if (oldGroup != newGroup) {
        // There is a change of group.
        if (oldGroup == nullptr) {
          // The tab was ungrouped and is moving to a group.
          web::WebState* currentWebState =
              _webStateList->GetWebStateAt(selectionOnlyChange.index());

          GridItemIdentifier* tabIdentifierToAddToGroup =
              [GridItemIdentifier tabIdentifier:currentWebState];

          [self.consumer removeItemWithIdentifier:tabIdentifierToAddToGroup
                           selectedItemIdentifier:[self activeIdentifier]];
        } else {
          // The tab left a group.
          GridItemIdentifier* oldGroupIdentifier =
              [GridItemIdentifier groupIdentifier:oldGroup
                                 withWebStateList:_webStateList];
          [self.consumer replaceItem:oldGroupIdentifier
                 withReplacementItem:oldGroupIdentifier];
        }

        if (newGroup) {
          // The tab joined a group.
          GridItemIdentifier* newGroupIdentifier =
              [GridItemIdentifier groupIdentifier:newGroup
                                 withWebStateList:_webStateList];

          [self.consumer replaceItem:newGroupIdentifier
                 withReplacementItem:newGroupIdentifier];
        } else {
          // The tab is now ungrouped.
          int webStateIndex = selectionOnlyChange.index();
          web::WebState* currentWebState =
              _webStateList->GetWebStateAt(webStateIndex);

          [self insertItem:[GridItemIdentifier tabIdentifier:currentWebState]
              beforeWebStateIndex:webStateIndex + 1];
        }

        // If the web state is the active one, the new group needs to be
        // highlighted.
        if (selectionOnlyChange.index() == webStateList->active_index()) {
          [self.consumer selectItemWithIdentifier:[self activeIdentifier]];
        }
        break;
      }
      // The activation is handled after this switch statement.
      break;
    }
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detachChange =
          change.As<WebStateListChangeDetach>();
      if (detachChange.group()) {
        [self updateCellGroup:detachChange.group()];
      }
      // Do not manage other case scenarios as this is already handled in
      // `-willChangeWebStateList:change:status:` function.
      break;
    }
    case WebStateListChange::Type::kMove: {
      const WebStateListChangeMove& moveChange =
          change.As<WebStateListChangeMove>();

      if (moveChange.pinned_state_changed()) {
        // The pinned state can be updated when a tab is moved.
        [self changePinnedStateForWebState:moveChange.moved_web_state()
                                   atIndex:moveChange.moved_to_index()
                              updatedGroup:moveChange.old_group()
                                               ? moveChange.old_group()
                                               : moveChange.new_group()];
      } else if (![self isPinnedWebState:moveChange.moved_to_index()]) {
        // BaseGridMediator handles only non pinned tabs because pinned tabs are
        // handled in PinnedTabsMediator.
        if (moveChange.old_group() == moveChange.new_group()) {
          // No group update.
          if (moveChange.old_group()) {
            // The cell moved inside its group, update the group.
            [self updateCellGroup:moveChange.old_group()];
          } else {
            // The cell is not in a group, move it.
            [self moveItem:[GridItemIdentifier
                               tabIdentifier:moveChange.moved_web_state()]
                beforeWebStateIndex:moveChange.moved_to_index() + 1];
          }
        } else {
          // The group has changed.
          if (moveChange.old_group()) {
            // The cell left a group.
            [self updateCellGroup:moveChange.old_group()];
          } else {
            // The cell wasn't in a group, remove it from the grid.
            [self.consumer removeItemWithIdentifier:
                               [GridItemIdentifier
                                   tabIdentifier:moveChange.moved_web_state()]
                             selectedItemIdentifier:[self activeIdentifier]];
          }
          if (moveChange.new_group()) {
            // The cell joined a group.
            [self updateCellGroup:moveChange.new_group()];
          } else {
            // The cell was removed from its group, add it to the grid.
            web::WebState* movedWebState = moveChange.moved_web_state();
            [self insertItem:[GridItemIdentifier tabIdentifier:movedWebState]
                beforeWebStateIndex:moveChange.moved_to_index() + 1];
          }
          // If the web state is the active one, the new group needs to be
          // highlighted.
          if (moveChange.moved_to_index() == webStateList->active_index()) {
            [self.consumer selectItemWithIdentifier:[self activeIdentifier]];
          }
        }
      }
      break;
    }
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replaceChange =
          change.As<WebStateListChangeReplace>();
      if ([self isPinnedWebState:replaceChange.index()]) {
        break;
      }
      web::WebState* replacedWebState = replaceChange.replaced_web_state();
      web::WebState* insertedWebState = replaceChange.inserted_web_state();
      [self.consumer replaceItem:[GridItemIdentifier
                                     tabIdentifier:replacedWebState]
             withReplacementItem:[GridItemIdentifier
                                     tabIdentifier:insertedWebState]];

      [self removeObservationForWebState:replacedWebState];
      [self addObservationForWebState:insertedWebState];
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insertChange =
          change.As<WebStateListChangeInsert>();
      if ([self isPinnedWebState:insertChange.index()]) {
        [self.consumer selectItemWithIdentifier:[self activeIdentifier]];
        break;
      }

      if (insertChange.group()) {
        [self updateCellGroup:insertChange.group()];
      } else {
        web::WebState* insertedWebState = insertChange.inserted_web_state();
        [self insertItem:[GridItemIdentifier tabIdentifier:insertedWebState]
            beforeWebStateIndex:insertChange.index() + 1];
      }
      [self addObservationForWebState:insertChange.inserted_web_state()];
      break;
    }
    case WebStateListChange::Type::kGroupCreate: {
      const WebStateListChangeGroupCreate& groupCreateChange =
          change.As<WebStateListChangeGroupCreate>();

      const TabGroup* currentGroup = groupCreateChange.created_group();
      GridItemIdentifier* groupItemIdentifier =
          [GridItemIdentifier groupIdentifier:currentGroup
                             withWebStateList:webStateList];
      CHECK(groupItemIdentifier.tabGroupItem.tabGroup);
      [self insertItem:groupItemIdentifier
          beforeWebStateIndex:groupItemIdentifier.tabGroupItem.tabGroup->range()
                                  .range_end() +
                              1];
      break;
    }
    case WebStateListChange::Type::kGroupVisualDataUpdate: {
      const WebStateListChangeGroupVisualDataUpdate& visualDataChange =
          change.As<WebStateListChangeGroupVisualDataUpdate>();
      GridItemIdentifier* groupItemIdentifier =
          [GridItemIdentifier groupIdentifier:visualDataChange.updated_group()
                             withWebStateList:webStateList];
      [self.consumer replaceItem:groupItemIdentifier
             withReplacementItem:groupItemIdentifier];

      break;
    }
    case WebStateListChange::Type::kGroupMove: {
      const WebStateListChangeGroupMove& groupMoveChange =
          change.As<WebStateListChangeGroupMove>();
      [self moveItem:[GridItemIdentifier
                          groupIdentifier:groupMoveChange.moved_group()
                         withWebStateList:webStateList]
          beforeWebStateIndex:groupMoveChange.moved_to_range().range_end()];
      break;
    }
    case WebStateListChange::Type::kGroupDelete: {
      const WebStateListChangeGroupDelete& groupDeleteChange =
          change.As<WebStateListChangeGroupDelete>();

      GridItemIdentifier* groupItemIdentifier =
          [GridItemIdentifier groupIdentifier:groupDeleteChange.deleted_group()
                             withWebStateList:_webStateList];
      [_selectedEditingItems removeItem:groupItemIdentifier];
      [self.consumer removeItemWithIdentifier:groupItemIdentifier
                       selectedItemIdentifier:[self activeIdentifier]];
      break;
    }
  }
  [self updateToolbarAfterNumberOfItemsChanged];
  if (status.active_web_state_change()) {
    [self.consumer selectItemWithIdentifier:[self activeIdentifier]];
  }
}

- (void)webStateListWillBeginBatchOperation:(WebStateList*)webStateList {
  DCHECK_EQ(_webStateList, webStateList);
  _scopedWebStateObservation->RemoveAllObservations();
}

- (void)webStateListBatchOperationEnded:(WebStateList*)webStateList {
  DCHECK_EQ(_webStateList, webStateList);

  // Clear selections.
  [_selectedEditingItems removeAllItems];

  [self addWebStateObservations];
  [self populateConsumerItems];
  [self updateToolbarAfterNumberOfItemsChanged];
}

#pragma mark - CRWWebStateObserver

- (void)webStateDidStartLoading:(web::WebState*)webState {
  [self updateConsumerItemForWebState:webState];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  [self updateConsumerItemForWebState:webState];
}

- (void)webStateDidChangeTitle:(web::WebState*)webState {
  [self updateConsumerItemForWebState:webState];
}

#pragma mark - SnapshotStorageObserver

- (void)didUpdateSnapshotStorageWithSnapshotID:(SnapshotIDWrapper*)snapshotID {
  web::WebState* webState = nullptr;
  WebStateList* webStateList = self.webStateList;
  for (int i = webStateList->pinned_tabs_count(); i < webStateList->count();
       i++) {
    SnapshotTabHelper* snapshotTabHelper =
        SnapshotTabHelper::FromWebState(webStateList->GetWebStateAt(i));
    if (snapshotID.snapshot_id == snapshotTabHelper->GetSnapshotID()) {
      webState = webStateList->GetWebStateAt(i);
      break;
    }
  }
  if (webState) {
    // It is possible to observe an updated snapshot for a WebState before
    // observing that the WebState has been added to the WebStateList. It is the
    // consumer's responsibility to ignore any updates before inserts.
    [self updateConsumerItemForWebState:webState];
  }
}

#pragma mark - GridCommands

- (BOOL)addNewItem {
  // The incognito mediator's Browser is briefly set to nil after the last
  // incognito tab is closed.
  if (!self.browser) {
    return NO;
  }

  if (self.browserState &&
      !IsAddNewTabAllowedByPolicy(self.browserState->GetPrefs(),
                                  self.browserState->IsOffTheRecord())) {
    return NO;
  }

  // The function is clamping the value, so it safe to pass the total count of
  // the WebState even if it is supposed to be a grid index.
  [self insertNewWebStateAtGridIndex:self.webStateList->count()
                             withURL:GURL(kChromeUINewTabURL)];
  return YES;
}

- (void)selectItemWithID:(web::WebStateID)itemID
                    pinned:(BOOL)pinned
    isFirstActionOnTabGrid:(BOOL)isFirstActionOnTabGrid {
  WebStateSearchCriteria searchCriteria{
      .identifier = itemID,
      .pinned_state = pinned ? PinnedState::kPinned : PinnedState::kNonPinned,
  };

  int index = GetWebStateIndex(self.webStateList, searchCriteria);
  WebStateList* itemWebStateList = self.webStateList;
  if (index == WebStateList::kInvalidIndex) {
    if (pinned) {
      return;
    }
    // If this is a search result, it may contain items from other windows or
    // from the inactive browser - check inactive browser and other windows
    // before giving up.
    BrowserList* browserList =
        BrowserListFactory::GetForBrowserState(self.browserState);
    Browser* browser = GetBrowserForNonPinnedTabWithId(
        browserList, itemID, self.browserState->IsOffTheRecord());

    if (!browser) {
      return;
    }

    if (browser->IsInactive()) {
      base::RecordAction(
          base::UserMetricsAction("MobileTabGridOpenInactiveTabSearchResult"));
      index = itemWebStateList->count();
      MoveTabToBrowser(itemID, self.browser, index);
    } else {
      // Other windows case.
      itemWebStateList = browser->GetWebStateList();
      index = GetWebStateIndex(itemWebStateList,
                               WebStateSearchCriteria{
                                   .identifier = itemID,
                                   .pinned_state = PinnedState::kNonPinned,
                               });
      SceneState* targetSceneState = browser->GetSceneState();
      SceneState* currentSceneState = self.browser->GetSceneState();

      UISceneActivationRequestOptions* options =
          [[UISceneActivationRequestOptions alloc] init];
      options.requestingScene = currentSceneState.scene;

      [[UIApplication sharedApplication]
          requestSceneSessionActivation:targetSceneState.scene.session
                           userActivity:nil
                                options:options
                           errorHandler:^(NSError* error) {
                             LOG(ERROR) << base::SysNSStringToUTF8(
                                 error.localizedDescription);
                             NOTREACHED_IN_MIGRATION();
                           }];
    }
  }

  web::WebState* selectedWebState = itemWebStateList->GetWebStateAt(index);
  LogPriceDropMetrics(selectedWebState);

  base::TimeDelta timeSinceLastActivation =
      base::Time::Now() - selectedWebState->GetLastActiveTime();
  base::UmaHistogramCustomTimes(
      "IOS.TabGrid.TabSelected.TimeSinceLastActivation",
      timeSinceLastActivation, base::Minutes(1), base::Days(24), 50);

  // Don't attempt a no-op activation. Normally this is not an issue, but it's
  // possible that this method (-selectItemWithID:) is being called as part of
  // a WebStateListObserver callback, in which case even a no-op activation
  // will cause a CHECK().
  if (selectedWebState == itemWebStateList->GetActiveWebState()) {
    // In search mode, the consumer doesn't have any information about the
    // selected item. So even if the active WebState is the same as the one that
    // is being selected, make sure that the consumer updates its selected item.
    [self.consumer
        selectItemWithIdentifier:[GridItemIdentifier
                                     tabIdentifier:selectedWebState]];
    return;
  } else {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridMoveToExistingTab"));
    if (isFirstActionOnTabGrid) {
      int activeWebStateIndex = itemWebStateList->active_index();
      BOOL adjacentTabSelected =
          std::abs(index - activeWebStateIndex) == 1 &&
          index != WebStateList::kInvalidIndex &&
          activeWebStateIndex != WebStateList::kInvalidIndex;
      if (adjacentTabSelected && self.browser) {
        TabBasedIPHBrowserAgent* tabBasedIPHBrowserAgent =
            TabBasedIPHBrowserAgent::FromBrowser(self.browser);
        tabBasedIPHBrowserAgent->NotifySwitchToAdjacentTabFromTabGrid();
      }
    }
  }

  // Avoid a reentrant activation. This is a fix for crbug.com/1134663, although
  // ignoring the selection at this point may do weird things.
  if (itemWebStateList->IsMutating()) {
    return;
  }

  // It should be safe to activate here.
  itemWebStateList->ActivateWebStateAt(index);
}

- (void)selectTabGroup:(const TabGroup*)tabGroup {
  WebStateList* webStateList = self.webStateList;

  if (webStateList->ContainsGroup(tabGroup)) {
    [self.tabGroupsHandler showTabGroup:tabGroup];
    return;
  }

  BOOL incognito = self.browserState->IsOffTheRecord();
  // If this is a search result, it may contain items from other windows or
  // from the inactive browser - check other windows before giving up.
  BrowserList* browserList =
      BrowserListFactory::GetForBrowserState(self.browserState);
  Browser* browser = GetBrowserForGroup(browserList, tabGroup, incognito);

  if (!browser) {
    return;
  }

  base::RecordAction(
      base::UserMetricsAction("MobileTabGridOpenSearchResultInAnotherWindow"));

  SceneState* targetSceneState = browser->GetSceneState();
  SceneState* currentSceneState = self.browser->GetSceneState();

  UISceneActivationRequestOptions* options =
      [[UISceneActivationRequestOptions alloc] init];
  options.requestingScene = currentSceneState.scene;

  [[UIApplication sharedApplication]
      requestSceneSessionActivation:targetSceneState.scene.session
                       userActivity:nil
                            options:options
                       errorHandler:^(NSError* error) {
                         LOG(ERROR) << base::SysNSStringToUTF8(
                             error.localizedDescription);
                         NOTREACHED_IN_MIGRATION();
                       }];

  if (!targetSceneState.UIEnabled) {
    return;
  }

  id<ApplicationCommands> applicationHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands);
  TabGridOpeningMode openingMode =
      incognito ? TabGridOpeningMode::kIncognito : TabGridOpeningMode::kRegular;
  [applicationHandler displayTabGridInMode:openingMode];

  id<TabGroupsCommands> tabGroupsHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), TabGroupsCommands);
  [tabGroupsHandler showTabGroup:tabGroup];
}

- (BOOL)isItemWithIDSelected:(web::WebStateID)itemID {
  int index = GetWebStateIndex(self.webStateList,
                               WebStateSearchCriteria{.identifier = itemID});
  if (index == WebStateList::kInvalidIndex) {
    return NO;
  }
  return index == self.webStateList->active_index();
}

- (void)setPinState:(BOOL)pinState forItemWithID:(web::WebStateID)itemID {
  SetWebStatePinnedState(self.webStateList, itemID, pinState);
}

- (void)closeItemWithID:(web::WebStateID)itemID {
  [self.tabGridIdleStatusHandler
      tabGridDidPerformAction:TabGridActionType::kInPageAction];
  int index = GetWebStateIndex(self.webStateList,
                               WebStateSearchCriteria{
                                   .identifier = itemID,
                               });
  if (index != WebStateList::kInvalidIndex) {
    self.webStateList->CloseWebStateAt(index, WebStateList::CLOSE_USER_ACTION);
    return;
  }

  TabSwitcherItem* itemToRemove =
      [[TabSwitcherItem alloc] initWithIdentifier:itemID];

  GridItemIdentifier* identifierToRemove =
      [[GridItemIdentifier alloc] initWithTabItem:itemToRemove];

  // `index` is `WebStateList::kInvalidIndex`, so `itemID` should be a search
  // result from a different window. Since this item is not from the current
  // browser, no UI updates will be sent to the current grid. Notify the current
  // grid consumer about the change.
  [self.consumer removeItemWithIdentifier:identifierToRemove
                   selectedItemIdentifier:nil];
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridSearchCloseTabFromAnotherWindow"));

  BrowserList* browserList =
      BrowserListFactory::GetForBrowserState(self.browserState);
  Browser* browser = GetBrowserForNonPinnedTabWithId(
      browserList, itemID, self.browserState->IsOffTheRecord());

  // If this tab is still associated with another browser, remove it from the
  // associated web state list.
  if (browser) {
    WebStateList* itemWebStateList = browser->GetWebStateList();
    index = GetWebStateIndex(itemWebStateList,
                             WebStateSearchCriteria{
                                 .identifier = itemID,
                                 .pinned_state = PinnedState::kNonPinned,
                             });
    itemWebStateList->CloseWebStateAt(index, WebStateList::CLOSE_USER_ACTION);
  }
}

- (void)closeItemsWithIDs:(const std::set<web::WebStateID>&)itemIDs {
  auto itemsCount = itemIDs.size();
  base::UmaHistogramCounts100("IOS.TabGrid.Selection.CloseTabs", itemsCount);
  RecordTabGridCloseTabsCount(itemsCount);

  WebStateList* webStateList = self.webStateList;
  {
    WebStateList::ScopedBatchOperation lock =
        webStateList->StartBatchOperation();
    for (const web::WebStateID itemID : itemIDs) {
      const int index = GetWebStateIndex(
          webStateList,
          WebStateSearchCriteria{.identifier = itemID,
                                 .pinned_state = PinnedState::kNonPinned});
      if (index != WebStateList::kInvalidIndex) {
        GridItemIdentifier* identifierToRemove = [GridItemIdentifier
            tabIdentifier:webStateList->GetWebStateAt(index)];
        [_selectedEditingItems removeItem:identifierToRemove];
        webStateList->CloseWebStateAt(index, WebStateList::CLOSE_USER_ACTION);
      }
    }
  }

  const bool allTabsClosed = webStateList->empty();
  if (allTabsClosed) {
    if (!self.browserState->IsOffTheRecord()) {
      base::RecordAction(base::UserMetricsAction(
          "MobileTabGridSelectionCloseAllRegularTabsConfirmed"));
    } else {
      base::RecordAction(base::UserMetricsAction(
          "MobileTabGridSelectionCloseAllIncognitoTabsConfirmed"));
    }
  }
}

- (void)deleteTabGroup:(const TabGroup*)group sourceView:(UIView*)sourceView {
  if (IsTabGroupSyncEnabled()) {
    [self.tabGroupsHandler
        showTabGroupConfirmationForAction:TabGroupActionType::kDeleteTabGroup
                                    group:group
                               sourceView:sourceView];
    return;
  }

  DCHECK(!IsTabGroupSyncEnabled());
  [self closeTabGroup:group andDeleteGroup:YES];
}

- (void)closeTabGroup:(const TabGroup*)group {
  [self closeTabGroup:group andDeleteGroup:NO];
}

- (void)ungroupTabGroup:(const TabGroup*)group sourceView:(UIView*)sourceView {
  if (IsTabGroupSyncEnabled()) {
    [self.tabGroupsHandler
        showTabGroupConfirmationForAction:TabGroupActionType::kUngroupTabGroup
                                    group:group
                               sourceView:sourceView];
    return;
  }

  DCHECK(!IsTabGroupSyncEnabled());
  [self ungroupTabGroup:group];
}

- (void)closeAllItems {
  NOTREACHED_NORETURN() << "Should be implemented in a subclass.";
}

- (void)saveAndCloseAllItems {
  NOTREACHED_NORETURN() << "Should be implemented in a subclass.";
}

- (void)undoCloseAllItems {
  NOTREACHED_NORETURN() << "Should be implemented in a subclass.";
}

- (void)discardSavedClosedItems {
  NOTREACHED_NORETURN() << "Should be implemented in a subclass.";
}

- (void)searchItemsWithText:(NSString*)searchText {
  TabsSearchService* searchService =
      TabsSearchServiceFactory::GetForBrowserState(self.browserState);
  const std::u16string& searchTerm = base::SysNSStringToUTF16(searchText);
  searchService->Search(
      searchTerm,
      base::BindOnce(^(
          std::vector<TabsSearchService::TabsSearchBrowserResults> results) {
        NSMutableArray* currentBrowserItems = [[NSMutableArray alloc] init];
        NSMutableArray* remainingItems = [[NSMutableArray alloc] init];
        for (const TabsSearchService::TabsSearchBrowserResults& browserResults :
             results) {
          if (IsTabGroupInGridEnabled()) {
            for (const TabGroup* group : browserResults.tab_groups) {
              GridItemIdentifier* item = [GridItemIdentifier
                   groupIdentifier:group
                  withWebStateList:browserResults.browser->GetWebStateList()];
              if (browserResults.browser == self.browser) {
                [currentBrowserItems addObject:item];
              } else {
                [remainingItems addObject:item];
              }
            }
          }

          for (web::WebState* webState : browserResults.web_states) {
            GridItemIdentifier* item =
                [GridItemIdentifier tabIdentifier:webState];

            if (browserResults.browser == self.browser) {
              [currentBrowserItems addObject:item];
            } else {
              [remainingItems addObject:item];
            }
          }
        }

        NSArray* allItems = nil;
        // If there are results from Browsers other than the current one,
        // append those results to the end.
        if (remainingItems.count) {
          allItems = [currentBrowserItems
              arrayByAddingObjectsFromArray:remainingItems];
        } else {
          allItems = currentBrowserItems;
        }

        [self.consumer populateItems:allItems selectedItemIdentifier:nil];
      }));
}

- (void)resetToAllItems {
  [self populateConsumerItems];
}

- (void)fetchSearchHistoryResultsCountForText:(NSString*)searchText
                                   completion:(void (^)(size_t))completion {
  TabsSearchService* search_service =
      TabsSearchServiceFactory::GetForBrowserState(self.browserState);
  const std::u16string& searchTerm = base::SysNSStringToUTF16(searchText);
  search_service->SearchHistory(searchTerm,
                                base::BindOnce(^(size_t resultCount) {
                                  completion(resultCount);
                                }));
}

#pragma mark - TabCollectionDragDropHandler

- (NSArray<UIDragItem*>*)allSelectedDragItems {
  NSMutableArray<UIDragItem*>* dragItems = [[NSMutableArray alloc] init];
  for (GridItemIdentifier* itemID in _selectedEditingItems.itemsIdentifiers) {
    switch (itemID.type) {
      case GridItemType::kInactiveTabsButton:
        // Inactive Tabs button is not dragable and not stored in
        // `_selectedEditingItems`.
        NOTREACHED_NORETURN();
      case GridItemType::kTab: {
        UIDragItem* dragItem =
            [self dragItemForItemWithID:itemID.tabSwitcherItem.identifier];
        if (dragItem) {
          [dragItems addObject:dragItem];
        }
        break;
      }
      case GridItemType::kGroup: {
        UIDragItem* dragItem =
            [self dragItemForTabGroupItem:itemID.tabGroupItem];
        if (dragItem) {
          [dragItems addObject:dragItem];
        }
        break;
      }
      case GridItemType::kSuggestedActions:
        // Suggested actions items are not dragable and not stored in
        // `_selectedEditingItems`.
        NOTREACHED_NORETURN();
    }
  }
  return dragItems;
}

- (UIDragItem*)dragItemForTabGroupItem:(TabGroupItem*)tabGroupItem {
  return CreateTabGroupDragItem(tabGroupItem.tabGroup, self.browserState);
}

- (UIDragItem*)dragItemForItem:(TabSwitcherItem*)item {
  return [self dragItemForItemWithID:item.identifier];
}

- (void)dragWillBeginForTabSwitcherItem:(TabSwitcherItem*)item {
  _dragItemID = item.identifier;
}

- (void)dragWillBeginForTabGroupItem:(TabSwitcherItem*)item {
  NOTREACHED_IN_MIGRATION();
}

- (void)dragSessionDidEnd {
  _dragItemID = web::WebStateID();
  // Update buttons as the number of items or the number of selected items might
  // have changed.
  [self.toolbarsMutator setButtonsEnabled:YES];
  [self configureToolbarsButtons];
}

- (UIDropOperation)dropOperationForDropSession:(id<UIDropSession>)session
                                       toIndex:(NSUInteger)destinationIndex {
  UIDragItem* dragItem = session.localDragSession.items.firstObject;

  // Tab move operations only originate from Chrome so a local object is used.
  // Local objects allow synchronous drops, whereas NSItemProvider only allows
  // asynchronous drops.
  if ([dragItem.localObject isKindOfClass:[TabInfo class]]) {
    TabInfo* tabInfo = static_cast<TabInfo*>(dragItem.localObject);
    if (tabInfo.browserState != self.browserState) {
      // Tabs from different profiles cannot be dropped.
      return UIDropOperationForbidden;
    }

    // If the dropped tab is from the same Chrome window and has been removed,
    // cancel the drop operation.
    if (_dragItemID == tabInfo.tabID &&
        GetWebStateIndex(self.webStateList,
                         WebStateSearchCriteria{
                             .identifier = tabInfo.tabID,
                             .pinned_state = PinnedState::kNonPinned,
                         }) == WebStateList::kInvalidIndex) {
      return UIDropOperationCancel;
    }
    if (self.browserState->IsOffTheRecord() == tabInfo.incognito) {
      return UIDropOperationMove;
    }

    // Tabs of different profiles (regular/incognito) cannot be dropped.
    return UIDropOperationForbidden;
  }
  if ([dragItem.localObject isKindOfClass:[TabGroupInfo class]]) {
    TabGroupInfo* tabGroupInfo =
        static_cast<TabGroupInfo*>(dragItem.localObject);
    if (tabGroupInfo.browserState != self.browserState) {
      // Tabs from different profiles cannot be dropped.
      return UIDropOperationForbidden;
    }
    if (self.browserState->IsOffTheRecord() == tabGroupInfo.incognito) {
      if (self.currentMode == TabGridModeGroup) {
        // Can't drop a group in a group.
        return UIDropOperationForbidden;
      }
      return UIDropOperationMove;
    }
    // Tabs of different profiles (regular/incognito) cannot be dropped.
    return UIDropOperationForbidden;
  }

  // All URLs originating from Chrome create a new tab (as opposed to moving a
  // tab).
  if ([dragItem.localObject isKindOfClass:[NSURL class]]) {
    return UIDropOperationCopy;
  }

  // URLs are accepted when drags originate from outside Chrome.
  NSArray<NSString*>* acceptableTypes = @[ UTTypeURL.identifier ];
  if ([session hasItemsConformingToTypeIdentifiers:acceptableTypes]) {
    return UIDropOperationCopy;
  }

  // Other UTI types such as image data or file data cannot be dropped.
  return UIDropOperationForbidden;
}

- (void)dropItem:(UIDragItem*)dragItem
               toIndex:(NSUInteger)destinationIndex
    fromSameCollection:(BOOL)fromSameCollection {
  WebStateList* webStateList = self.webStateList;

  // Tab move operations only originate from Chrome so a local object is used.
  // Local objects allow synchronous drops, whereas NSItemProvider only allows
  // asynchronous drops.
  if ([dragItem.localObject isKindOfClass:[TabInfo class]]) {
    TabInfo* tabInfo = static_cast<TabInfo*>(dragItem.localObject);

    if (IsPinnedTabsEnabled()) {
      // Try to unpin the tab, if not pinned nothing happens.
      SetWebStatePinnedState(webStateList, tabInfo.tabID,
                             /*pin_state=*/false);
    }

    int sourceWebStateIndex =
        GetWebStateIndex(webStateList, WebStateSearchCriteria{
                                           .identifier = tabInfo.tabID,
                                       });

    if (sourceWebStateIndex == WebStateList::kInvalidIndex) {
      // Move tab across Browsers.
      base::UmaHistogramEnumeration(kUmaGridViewDragOrigin,
                                    DragItemOrigin::kOtherBrowser);
      int destinationWebStateIndex =
          WebStateIndexFromGridDropItemIndex(webStateList, destinationIndex);

      MoveTabToBrowser(tabInfo.tabID, self.browser, destinationWebStateIndex);
      return;
    }

    if (fromSameCollection) {
      base::UmaHistogramEnumeration(kUmaGridViewDragOrigin,
                                    DragItemOrigin::kSameCollection);
    } else {
      base::UmaHistogramEnumeration(kUmaGridViewDragOrigin,
                                    DragItemOrigin::kSameBrowser);
    }

    // Reorder tabs.
    int destinationWebStateIndex = WebStateIndexFromGridDropItemIndex(
        webStateList, destinationIndex, sourceWebStateIndex);
    const auto insertionParams =
        WebStateList::InsertionParams::AtIndex(destinationWebStateIndex);
    MoveWebStateWithIdentifierToInsertionParams(
        tabInfo.tabID, insertionParams, webStateList, fromSameCollection);
    return;
  }

  if ([dragItem.localObject isKindOfClass:[TabGroupInfo class]]) {
    TabGroupInfo* tabGroupInfo =
        static_cast<TabGroupInfo*>(dragItem.localObject);
    // Early return if the group has been closed during the drag an drop.
    if (!tabGroupInfo.tabGroup) {
      return;
    }
    if (fromSameCollection) {
      base::UmaHistogramEnumeration(kUmaGridViewDragOrigin,
                                    DragItemOrigin::kSameCollection);
      CHECK(tabGroupInfo.tabGroup);
      int sourceIndex = tabGroupInfo.tabGroup->range().range_begin();
      int nextWebStateIndex = WebStateIndexAfterGridDropItemIndex(
          webStateList, destinationIndex, sourceIndex);
      webStateList->MoveGroup(tabGroupInfo.tabGroup, nextWebStateIndex);
      return;
    } else {
      base::UmaHistogramEnumeration(kUmaGridViewDragOrigin,
                                    DragItemOrigin::kOtherBrowser);
    }

    int destinationWebStateIndex =
        WebStateIndexAfterGridDropItemIndex(webStateList, destinationIndex);
    tab_groups::utils::MoveTabGroupToBrowser(
        tabGroupInfo.tabGroup, self.browser, destinationWebStateIndex);
  }
  base::UmaHistogramEnumeration(kUmaGridViewDragOrigin, DragItemOrigin::kOther);

  // Handle URLs from within Chrome synchronously using a local object.
  if ([dragItem.localObject isKindOfClass:[URLInfo class]]) {
    URLInfo* droppedURL = static_cast<URLInfo*>(dragItem.localObject);
    [self insertNewWebStateAtGridIndex:destinationIndex withURL:droppedURL.URL];
    return;
  }
}

- (void)dropItemFromProvider:(NSItemProvider*)itemProvider
                     toIndex:(NSUInteger)destinationIndex
          placeholderContext:
              (id<UICollectionViewDropPlaceholderContext>)placeholderContext {
  if (![itemProvider canLoadObjectOfClass:[NSURL class]]) {
    [placeholderContext deletePlaceholder];
    return;
  }

  if (_currentMode == TabGridModeGroup) {
    base::UmaHistogramEnumeration(kUmaGroupViewDragOrigin,
                                  DragItemOrigin::kOther);
  } else {
    base::UmaHistogramEnumeration(kUmaGridViewDragOrigin,
                                  DragItemOrigin::kOther);
  }

  __weak BaseGridMediator* weakSelf = self;
  auto loadHandler =
      ^(__kindof id<NSItemProviderReading> providedItem, NSError* error) {
        dispatch_async(dispatch_get_main_queue(), ^{
          [placeholderContext deletePlaceholder];
          NSURL* droppedURL = static_cast<NSURL*>(providedItem);
          [weakSelf
              insertNewWebStateAtGridIndex:destinationIndex
                                   withURL:net::GURLWithNSURL(droppedURL)];
        });
      };
  [itemProvider loadObjectOfClass:[NSURL class] completionHandler:loadHandler];
}

#pragma mark - Private

// Returns a SnapshotStorageWrapper for the current browser.
- (SnapshotStorageWrapper*)snapshotStorage {
  if (!self.browser) {
    return nil;
  }
  return SnapshotBrowserAgent::FromBrowser(self.browser)->snapshot_storage();
}

- (void)addItemsWithIDsToReadingList:(const std::set<web::WebStateID>&)itemIDs {
  [self.delegate dismissPopovers];

  base::UmaHistogramCounts100("IOS.TabGrid.Selection.AddToReadingList",
                              itemIDs.size());

  NSArray<URLWithTitle*>* URLs = [self urlsWithTitleFromItemIDs:itemIDs];

  ReadingListAddCommand* command =
      [[ReadingListAddCommand alloc] initWithURLs:URLs];
  ReadingListBrowserAgent* readingListBrowserAgent =
      ReadingListBrowserAgent::FromBrowser(self.browser);
  readingListBrowserAgent->AddURLsToReadingList(command.URLs);
}

- (void)addItemsWithIDsToBookmarks:(const std::set<web::WebStateID>&)itemIDs {
  id<BookmarksCommands> bookmarkHandler =
      HandlerForProtocol(_browser->GetCommandDispatcher(), BookmarksCommands);

  if (!bookmarkHandler) {
    return;
  }
  [self.delegate dismissPopovers];
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridAddedMultipleNewBookmarks"));
  base::UmaHistogramCounts100("IOS.TabGrid.Selection.AddToBookmarks",
                              itemIDs.size());

  NSArray<URLWithTitle*>* URLs = [self urlsWithTitleFromItemIDs:itemIDs];

  [bookmarkHandler bookmarkWithFolderChooser:URLs];
}

- (NSArray<URLWithTitle*>*)urlsWithTitleFromItemIDs:
    (const std::set<web::WebStateID>&)itemIDs {
  NSMutableArray<URLWithTitle*>* URLs = [[NSMutableArray alloc] init];
  for (const web::WebStateID itemID : itemIDs) {
    TabItem* item = GetTabItem(self.webStateList,
                               WebStateSearchCriteria{
                                   .identifier = itemID,
                                   .pinned_state = PinnedState::kNonPinned,
                               });
    URLWithTitle* URL = [[URLWithTitle alloc] initWithURL:item.URL
                                                    title:item.title];
    [URLs addObject:URL];
  }
  return URLs;
}

// Inserts/removes a non pinned item to/from the collection.
- (void)changePinnedStateForWebState:(web::WebState*)webState
                             atIndex:(int)index
                        updatedGroup:(const TabGroup*)group {
  if ([self isPinnedWebState:index]) {
    if (group) {
      [self updateCellGroup:group];
    } else {
      GridItemIdentifier* identifierToRemove =
          [GridItemIdentifier tabIdentifier:webState];
      [self.consumer removeItemWithIdentifier:identifierToRemove
                       selectedItemIdentifier:[self activeIdentifier]];
    }
    [self removeObservationForWebState:webState];
  } else {
    if (group) {
      [self updateCellGroup:group];
    } else {
      [self insertItem:[GridItemIdentifier tabIdentifier:webState]
          beforeWebStateIndex:index + 1];
    }
    [self addObservationForWebState:webState];
  }
}

- (BOOL)isPinnedWebState:(int)index {
  if (IsPinnedTabsEnabled() && self.webStateList->IsWebStatePinnedAt(index)) {
    return YES;
  }
  return NO;
}

// Updates toolbars when the number of web state might be changed.
- (void)updateToolbarAfterNumberOfItemsChanged {
  if (self.currentMode == TabGridModeSelection && self.webStateList->empty()) {
    // Exit selection mode if there are no more tabs.
    self.currentMode = TabGridModeNormal;
  } else {
    // Update toolbar's buttons as the number of tabs have probably changed so
    // the options changed (ex: "Undo" may be available now).
    [self configureToolbarsButtons];
  }
}

// Returns a drag item for the given `itemID`.
- (UIDragItem*)dragItemForItemWithID:(web::WebStateID)itemID {
  web::WebState* webState = GetWebState(
      self.webStateList, WebStateSearchCriteria{
                             .identifier = itemID,
                             .pinned_state = PinnedState::kNonPinned,
                         });
  return CreateTabDragItem(webState);
}

// Returns the menu to display when the Add To button is selected for `items`.
- (NSArray<UIMenuElement*>*)addToButtonMenuElements {
  if (!self.browser) {
    return nil;
  }

  NSMutableArray<UIMenuElement*>* actions = [[NSMutableArray alloc] init];

  ActionFactory* actionFactory = [[ActionFactory alloc]
      initWithScenario:kMenuScenarioHistogramTabGridAddTo];

  __weak BaseGridMediator* weakSelf = self;

  if (IsTabGroupInGridEnabled()) {
    auto addToGroupBlock = ^(const TabGroup* group) {
      [weakSelf addSelectedElementsToGroup:group];
    };
    UIMenuElement* addToGroup = [actionFactory
        menuToAddTabToGroupWithGroups:GetAllGroupsForBrowserState(_browserState)
                         numberOfTabs:_selectedEditingItems.tabsCount
                                block:addToGroupBlock];
    [actions addObject:[UIMenu menuWithTitle:@""
                                       image:nil
                                  identifier:nil
                                     options:UIMenuOptionsDisplayInline
                                    children:@[ addToGroup ]]];
  }

  // Copy the set of items, so that the following block can use it.
  std::set<web::WebStateID> shareableTabsCopy =
      [_selectedEditingItems sharableTabs];

  UIAction* addToReadingListAction =
      [actionFactory actionToAddToReadingListWithBlock:^{
        [weakSelf addItemsWithIDsToReadingList:shareableTabsCopy];
      }];

  UIAction* bookmarkAction = [actionFactory actionToBookmarkWithBlock:^{
    [weakSelf addItemsWithIDsToBookmarks:shareableTabsCopy];
  }];
  // Bookmarking can be disabled from prefs (from an enterprise policy),
  // if that's the case grey out the option in the menu.
  BOOL isEditBookmarksEnabled =
      self.browser->GetBrowserState()->GetPrefs()->GetBoolean(
          bookmarks::prefs::kEditBookmarksEnabled);
  if (!isEditBookmarksEnabled) {
    bookmarkAction.attributes = UIMenuElementAttributesDisabled;
  }
  if (shareableTabsCopy.size() == 0) {
    addToReadingListAction.attributes = UIMenuElementAttributesDisabled;
    bookmarkAction.attributes = UIMenuElementAttributesDisabled;
  }

  [actions addObject:addToReadingListAction];
  [actions addObject:bookmarkAction];

  return actions;
}

// Adds all the current selected elements to `group`. Pass nullptr to add to a
// new group.
- (void)addSelectedElementsToGroup:(const TabGroup*)group {
  std::set<web::WebStateID> selectedTabs = [_selectedEditingItems allTabs];
  if (group == nullptr) {
    [self.tabGroupsHandler showTabGroupCreationForTabs:selectedTabs];
  } else {
    WebStateList::ScopedBatchOperation lock =
        self.webStateList->StartBatchOperation();
    for (web::WebStateID webStateID : selectedTabs) {
      MoveTabToGroup(webStateID, group, _browserState);
    }
  }
}

// Returns the associated WebStateList for the given `group`.
- (WebStateList*)groupWebStateList:(const TabGroup*)group {
  if (_webStateList->ContainsGroup(group)) {
    return _webStateList;
  }
  BrowserList* browserList =
      BrowserListFactory::GetForBrowserState(self.browserState);
  Browser* browser = GetBrowserForGroup(browserList, group,
                                        self.browserState->IsOffTheRecord());
  if (!browser) {
    return nullptr;
  }
  return browser->GetWebStateList();
}

// Updates the cell of the given `group`.
- (void)updateCellGroup:(const TabGroup*)group {
  GridItemIdentifier* groupIdentifier =
      [GridItemIdentifier groupIdentifier:group
                         withWebStateList:self.webStateList];
  [self.consumer replaceItem:groupIdentifier
         withReplacementItem:groupIdentifier];
}

#pragma mark - TabGridPageMutator

- (void)currentlySelectedGrid:(BOOL)selected {
  NOTREACHED_NORETURN() << "Should be implemented in a subclass.";
}

- (void)switchToMode:(TabGridMode)mode {
  self.currentMode = mode;
}

#pragma mark - TabGridToolbarsGridDelegate

- (void)closeAllButtonTapped:(id)sender {
  NOTREACHED_NORETURN() << "Should be implemented in a subclass.";
}

- (void)doneButtonTapped:(id)sender {
  // Tapping Done when in selection mode, should only return back to the normal
  // mode.
  if (self.currentMode == TabGridModeSelection) {
    self.currentMode = TabGridModeNormal;
    // Records action when user exit the selection mode.
    base::RecordAction(base::UserMetricsAction("MobileTabGridSelectionDone"));
  } else {
    [self.toolbarTabGridDelegate doneButtonTapped:sender];
  }
}

- (void)newTabButtonTapped:(id)sender {
  NOTREACHED_NORETURN() << "Should be implemented in a subclass.";
}

- (void)selectAllButtonTapped:(id)sender {
  NSUInteger selectedItemsCount = _selectedEditingItems.tabsCount;
  BOOL allItemsSelected =
      static_cast<int>(selectedItemsCount) ==
      (self.webStateList->count() - self.webStateList->pinned_tabs_count());

  // Deselect all items if they are all already selected.
  if (allItemsSelected) {
    [_selectedEditingItems removeAllItems];
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridSelectionDeselectAll"));
  } else {
    NSArray<GridItemIdentifier*>* identifiers = CreateItems(self.webStateList);
    for (GridItemIdentifier* identifier in identifiers) {
      [self addToSelectionItemID:identifier];
    }
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridSelectionSelectAll"));
  }
  [self.consumer reload];
  [self configureToolbarsButtons];
}

- (void)searchButtonTapped:(id)sender {
  self.currentMode = TabGridModeSearch;
  base::RecordAction(base::UserMetricsAction("MobileTabGridSearchTabs"));
}

- (void)cancelSearchButtonTapped:(id)sender {
  base::RecordAction(base::UserMetricsAction("MobileTabGridCancelSearchTabs"));
  self.currentMode = TabGridModeNormal;
}

- (void)closeSelectedTabs:(id)sender {
  [self.delegate dismissPopovers];

  std::set<web::WebStateID> selectedIDs;
  for (GridItemIdentifier* identifier in _selectedEditingItems
           .itemsIdentifiers) {
    switch (identifier.type) {
      case GridItemType::kInactiveTabsButton:
        NOTREACHED_NORETURN();
      case GridItemType::kTab:
        selectedIDs.insert(identifier.tabSwitcherItem.identifier);
        break;
      case GridItemType::kGroup: {
        CHECK(identifier.tabGroupItem.tabGroup);
        const TabGroupRange groupRange =
            identifier.tabGroupItem.tabGroup->range();
        for (int index : groupRange) {
          web::WebState* webState = self.webStateList->GetWebStateAt(index);
          selectedIDs.insert(webState->GetUniqueIdentifier());
        }
        break;
      }
      case GridItemType::kSuggestedActions:
        NOTREACHED_NORETURN();
    }
  }
  [self.delegate
      showCloseItemsConfirmationActionSheetWithBaseGridMediator:self
                                                        itemIDs:selectedIDs
                                                         anchor:sender];
}

- (void)shareSelectedTabs:(id)sender {
  [self.delegate dismissPopovers];

  base::RecordAction(
      base::UserMetricsAction("MobileTabGridSelectionShareTabs"));
  base::UmaHistogramCounts100("IOS.TabGrid.Selection.ShareTabs",
                              _selectedEditingItems.sharableTabsCount);
  [self.delegate baseGridMediator:self
                        shareURLs:[_selectedEditingItems selectedTabsURLs]
                           anchor:sender];
}

- (void)selectTabsButtonTapped:(id)sender {
  self.currentMode = TabGridModeSelection;
  [self configureToolbarsButtons];
  base::RecordAction(base::UserMetricsAction("MobileTabGridSelectTabs"));
}

#pragma mark - GridViewControllerMutator

- (void)userTappedOnItemID:(GridItemIdentifier*)itemID {
  CHECK(itemID.type == GridItemType::kInactiveTabsButton ||
        itemID.type == GridItemType::kGroup ||
        itemID.type == GridItemType::kTab);
  if (self.currentMode == TabGridModeSelection) {
    CHECK(itemID.type != GridItemType::kInactiveTabsButton);
    if ([self isItemSelected:itemID]) {
      [self removeFromSelectionItemID:itemID];
    } else {
      [self addToSelectionItemID:itemID];
    }
  }
}

- (void)addToSelectionItemID:(GridItemIdentifier*)itemID {
  CHECK(itemID.type == GridItemType::kTab ||
        itemID.type == GridItemType::kGroup);
  if (self.currentMode != TabGridModeSelection) {
    base::debug::DumpWithoutCrashing();
    return;
  }
  [_selectedEditingItems addItem:itemID];
  [self configureToolbarsButtons];
}

- (void)removeFromSelectionItemID:(GridItemIdentifier*)itemID {
  CHECK(itemID.type == GridItemType::kTab ||
        itemID.type == GridItemType::kGroup);
  if (self.currentMode != TabGridModeSelection) {
    return;
  }

  [_selectedEditingItems removeItem:itemID];
  [self configureToolbarsButtons];
}

- (void)closeItemWithIdentifier:(GridItemIdentifier*)identifier {
  switch (identifier.type) {
    case GridItemType::kInactiveTabsButton:
      NOTREACHED_NORETURN();
    case GridItemType::kTab:
      [self closeItemWithID:identifier.tabSwitcherItem.identifier];
      break;
    case GridItemType::kGroup: {
      const TabGroup* group = identifier.tabGroupItem.tabGroup;
      [self closeTabGroup:group andDeleteGroup:NO];
      break;
    }
    case GridItemType::kSuggestedActions:
      NOTREACHED_NORETURN();
  }
}

#pragma mark - BaseGridMediatorItemProvider

- (BOOL)isItemSelected:(GridItemIdentifier*)itemID {
  return [_selectedEditingItems containItem:itemID];
}

@end
