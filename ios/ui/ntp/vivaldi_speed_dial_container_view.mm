// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/vivaldi_speed_dial_container_view.h"

#import "UIKit/UIKit.h"

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/direct_match/direct_match_service.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/direct_match/direct_match_service_bridge.h"
#import "ios/ui/custom_views/custom_views_swift.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/ntp/cells/vivaldi_speed_dial_folder_list_cell.h"
#import "ios/ui/ntp/cells/vivaldi_speed_dial_folder_regular_cell.h"
#import "ios/ui/ntp/cells/vivaldi_speed_dial_list_cell.h"
#import "ios/ui/ntp/cells/vivaldi_speed_dial_regular_cell.h"
#import "ios/ui/ntp/cells/vivaldi_speed_dial_small_cell.h"
#import "ios/ui/ntp/vivaldi_ntp_constants.h"
#import "ios/ui/ntp/vivaldi_speed_dial_add_group_view.h"
#import "ios/ui/ntp/vivaldi_speed_dial_constants.h"
#import "ios/ui/ntp/vivaldi_speed_dial_container_view_flow_layout.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using l10n_util::GetNSString;

// NAMESPACE
namespace {
// Cell Identifier for the cell types
NSString* cellIdFolderRegular = @"cellIdFolderRegular";
NSString* cellIdFolderList = @"cellIdFolderList";
NSString* cellIdRegular = @"cellIdRegular";
NSString* cellIdSmall = @"cellIdSmall";
NSString* cellIdList = @"cellIdList";

NSString* syncedStoreURLKey = @"synced-store";

const CGFloat commonPadding = 20;
}

@interface VivaldiSpeedDialContainerView() <UICollectionViewDataSource,
                                            UICollectionViewDelegate,
                                            UICollectionViewDragDelegate,
                                            UICollectionViewDropDelegate,
                                     VivaldiSpeedDialAddGroupViewDelegate,
                                          DirectMatchServiceBridgeObserver,
                                             UIGestureRecognizerDelegate> {
  // Bridge to register for direct match service backend changes.
  std::unique_ptr<direct_match_ios::DirectMatchServiceBridge> _bridge;

  direct_match::DirectMatchService* _directMatchService;
}
// Action factory for speed dials
@property (nonatomic,strong) BrowserActionFactory* actionFactory;
// FaviconLoader is a keyed service that uses LargeIconService to retrieve
// favicon images.
@property(nonatomic,assign) FaviconLoader* faviconLoader;
// Collection view that holds the speed dial folder's children
@property(weak,nonatomic) UICollectionView *collectionView;
// Empty message view when there's no items available to show
@property(weak,nonatomic) VivaldiEmptyMessageView *emptyMessageView;
// Add group view visible when no items present or on last slide.
@property(weak,nonatomic) VivaldiSpeedDialAddGroupView *addGroupView;
// Collection view layout for selected column and layout rendering
@property(nonatomic,strong) VivaldiSpeedDialViewContainerViewFlowLayout *layout;
// Parent speed dial folder
@property(assign,nonatomic) VivaldiSpeedDialItem* parent;
// Array to store the children to populate on the collection view
@property(strong,nonatomic) NSMutableArray *speedDialItems;
// Currently selected layout
@property(nonatomic,assign) VivaldiStartPageLayoutStyle selectedLayout;
// Currently selected maximum columns
@property(nonatomic,assign) VivaldiStartPageLayoutColumn selectedColumn;
// Boolean to keep track if container is holding frequently visited items
@property(nonatomic,assign) BOOL showingFrequentlyVisited;
// Bool to keep track if top sites result is ready. The results can be empty,
// this only checks if we got a response from backend for the query.
@property(nonatomic,assign) BOOL isTopSitesResultsAvailable;
@end

@implementation VivaldiSpeedDialContainerView

@synthesize faviconLoader = _faviconLoader;
@synthesize collectionView = _collectionView;
@synthesize emptyMessageView = _emptyMessageView;
@synthesize parent = _parent;
@synthesize speedDialItems = _speedDialItems;

#pragma mark - INITIALIZER
- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    [self setUpUI];
    [self setUpTapGesture];
    [self startObservingSDItemPropertyChange];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark - SET UP UI COMPONENTS
- (void)setUpUI {
  VivaldiSpeedDialViewContainerViewFlowLayout *layout =
      [VivaldiSpeedDialViewContainerViewFlowLayout new];
  layout.layoutState = VivaldiStartPageLayoutStateNormal;
  layout.shouldShowTabletLayout = [self showTabletLayout];
  self.layout = layout;
  UICollectionView* collectionView =
    [[UICollectionView alloc] initWithFrame:CGRectZero
                       collectionViewLayout:layout];
  _collectionView = collectionView;
  [_collectionView setDataSource: self];
  [_collectionView setDelegate: self];
  [_collectionView setDropDelegate: self];
  [_collectionView setDragDelegate: self];
  _collectionView.dragInteractionEnabled = YES;
  _collectionView.showsHorizontalScrollIndicator = NO;
  _collectionView.showsVerticalScrollIndicator = NO;
  _collectionView.contentInsetAdjustmentBehavior =
    UIScrollViewContentInsetAdjustmentNever;

  [_collectionView registerClass:[VivaldiSpeedDialRegularCell class]
      forCellWithReuseIdentifier:cellIdRegular];
  [collectionView registerClass:[VivaldiSpeedDialSmallCell class]
      forCellWithReuseIdentifier:cellIdSmall];
  [collectionView registerClass:[VivaldiSpeedDialListCell class]
      forCellWithReuseIdentifier:cellIdList];
  [_collectionView registerClass:[VivaldiSpeedDialFolderRegularCell class]
      forCellWithReuseIdentifier:cellIdFolderRegular];
   [_collectionView registerClass:[VivaldiSpeedDialFolderListCell class]
       forCellWithReuseIdentifier:cellIdFolderList];

  [_collectionView setBackgroundColor:[UIColor clearColor]];

  [self addSubview:_collectionView];
  [_collectionView fillSuperview];
  _collectionView.hidden = YES;

  // Add group view
  VivaldiSpeedDialAddGroupView* addGroupView =
      [VivaldiSpeedDialAddGroupView new];
  addGroupView.translatesAutoresizingMaskIntoConstraints = NO;
  self.addGroupView = addGroupView;
  addGroupView.delegate = self;
  [self addSubview:addGroupView];

  [NSLayoutConstraint activateConstraints:@[
    [addGroupView.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [addGroupView.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
    [addGroupView.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.leadingAnchor
                                    constant:commonPadding],
    [addGroupView.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.trailingAnchor
                                 constant:-commonPadding]
  ]];
  addGroupView.hidden = YES;

  // Empty message view
  NSString* emptyString =
      GetNSString(IDS_IOS_START_PAGE_FREQUENTLY_VISITED_EMPTY_DESCRIPTION);
  VivaldiEmptyMessageView* emptyMessageView =
      [[VivaldiEmptyMessageView alloc] initWithMessage:emptyString];
  self.emptyMessageView = emptyMessageView;
  [self addSubview:emptyMessageView];
  [emptyMessageView fillSuperview];
  emptyMessageView.hidden = YES;
}

#pragma mark - SETTERS
- (void)configureActionFactory:(BrowserActionFactory*)actionFactory {
  self.actionFactory = actionFactory;
}

- (void)configureWith:(NSArray*)speedDials
               parent:(VivaldiSpeedDialItem*)parent
        faviconLoader:(FaviconLoader*)faviconLoader
   directMatchService:(direct_match::DirectMatchService*)directMatchService
          layoutStyle:(VivaldiStartPageLayoutStyle)style
         layoutColumn:(VivaldiStartPageLayoutColumn)column
         showAddGroup:(BOOL)showAddGroup
    frequentlyVisited:(BOOL)frequentlyVisited
    topSitesAvailable:(BOOL)topSitesAvailable
     topToolbarHidden:(BOOL)topToolbarHidden
    verticalSizeClass:(UIUserInterfaceSizeClass)verticalSizeClass {
  self.parent = parent;
  self.isTopSitesResultsAvailable = topSitesAvailable;
  self.showingFrequentlyVisited = frequentlyVisited;
  self.faviconLoader = faviconLoader;
  _directMatchService = directMatchService;
  _bridge.reset(new direct_match_ios::DirectMatchServiceBridge(
      self, directMatchService));
  self.selectedLayout = style;
  self.selectedColumn = column;
  self.layout.layoutStyle = style;
  self.layout.shouldShowTabletLayout = [self showTabletLayout];
  self.layout.numberOfColumns = column;
  self.layout.topToolbarHidden = topToolbarHidden;
  self.speedDialItems = [[NSMutableArray alloc] initWithArray:speedDials];
  self.collectionView.hidden = showAddGroup;
  self.addGroupView.hidden = !showAddGroup;
  self.emptyMessageView.hidden =
      !(frequentlyVisited && topSitesAvailable &&
        self.speedDialItems.count == 0);
  [self.collectionView reloadData];

  [self.addGroupView refreshLayoutWithVerticalSizeClass:verticalSizeClass];
  [self.addGroupView setNeedsLayout];
}

- (void)reloadLayoutWithStyle:(VivaldiStartPageLayoutStyle)style
                 layoutColumn:(VivaldiStartPageLayoutColumn)column {
  self.selectedLayout = style;
  self.selectedColumn = column;
  self.layout.layoutStyle = style;
  self.layout.numberOfColumns = column;
  [self.collectionView.collectionViewLayout invalidateLayout];
  [self.collectionView reloadData];
}

#pragma mark - VivaldiSpeedDialAddGroupViewDelegate
- (void)didTapAddNewGroup {
  if (self.delegate) {
    [self.delegate didSelectAddNewGroupForParent:self.parent];
  }
}

#pragma mark - COLLECTIONVIEW DATA SOURCE

- (NSInteger)collectionView:(UICollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section {
  // '+1' is for the 'New Speed Dial' item on the start page that's reponsible
  // for opening bookmark/speed dial editor.
  // Do not show that add item tile when frequently visited pages are visible.
  return self.speedDialItems.count + (self.showingFrequentlyVisited ? 0 : 1);
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {

  // If there's no more object then load the new speed dial creation tile
  if (indexPath.row == (long)self.speedDialItems.count) {
    switch (_selectedLayout) {
      case VivaldiStartPageLayoutStyleLarge:
      case VivaldiStartPageLayoutStyleMedium:
      case VivaldiStartPageLayoutStyleSmall: {
        VivaldiSpeedDialFolderRegularCell *folderRegularCell =
        [collectionView
          dequeueReusableCellWithReuseIdentifier:cellIdFolderRegular
                                    forIndexPath:indexPath];
        [folderRegularCell configureCellWith:nil
                                      addNew:YES
                                 layoutStyle:self.selectedLayout];
        return folderRegularCell;
      }
      case VivaldiStartPageLayoutStyleList: {
        VivaldiSpeedDialFolderListCell *folderListCell =
            [collectionView
              dequeueReusableCellWithReuseIdentifier:cellIdFolderList
                                        forIndexPath:indexPath];
        [folderListCell configureCellWith:nil
                                   addNew:YES];
        return folderListCell;
      }
    }
  }

  // Deque speed dial items
  VivaldiSpeedDialItem* item =
    [self.speedDialItems objectAtIndex: indexPath.row];

  if (item.isFolder) {
    switch (_selectedLayout) {
      case VivaldiStartPageLayoutStyleLarge:
      case VivaldiStartPageLayoutStyleMedium:
      case VivaldiStartPageLayoutStyleSmall: {
        VivaldiSpeedDialFolderRegularCell *folderRegularCell =
        [collectionView
          dequeueReusableCellWithReuseIdentifier:cellIdFolderRegular
                                    forIndexPath:indexPath];
        [folderRegularCell configureCellWith:item
                                      addNew:NO
                                 layoutStyle:self.selectedLayout];
        return folderRegularCell;
      }
      case VivaldiStartPageLayoutStyleList: {
        VivaldiSpeedDialFolderListCell *folderListCell =
            [collectionView
              dequeueReusableCellWithReuseIdentifier:cellIdFolderList
                                        forIndexPath:indexPath];
        [folderListCell configureCellWith:item
                                   addNew:NO];
        return folderListCell;
      }
    }
  } else {
    switch (_selectedLayout) {
      case VivaldiStartPageLayoutStyleLarge:
      case VivaldiStartPageLayoutStyleMedium: {
        VivaldiSpeedDialRegularCell *largeCell =
          [collectionView dequeueReusableCellWithReuseIdentifier:cellIdRegular
                                                    forIndexPath:indexPath];
        [largeCell configureCellWith:item layoutStyle:self.selectedLayout];
        [largeCell
            setActivityIndicatorLoading:item.isThumbnailRefreshing];
        [self loadFaviconForItem:item
                         forCell:largeCell];
        return largeCell;
      }
      case VivaldiStartPageLayoutStyleSmall: {
        VivaldiSpeedDialSmallCell *smallCell =
          [collectionView dequeueReusableCellWithReuseIdentifier:cellIdSmall
                                                    forIndexPath:indexPath];
        [smallCell configureCellWith:item
                            isTablet:self.showTabletLayout];
        [self loadFaviconForItem:item
                         forCell:smallCell];
        return smallCell;
      }
      case VivaldiStartPageLayoutStyleList: {
          VivaldiSpeedDialListCell *listCell =
            [collectionView dequeueReusableCellWithReuseIdentifier:cellIdList
                                                      forIndexPath:indexPath];
        [listCell configureCellWith:item];
        [self loadFaviconForItem:item
                         forCell:listCell];
        return listCell;
      }
    }
  }
}

// Asynchronously loads favicon for given index path. The loads are cancelled
// upon cell reuse automatically.  When the favicon is not found in cache,
// use the fall back icon style.
- (void)loadFaviconForItem:(VivaldiSpeedDialItem*)item
                   forCell:(UICollectionViewCell*)cell {

  CGFloat desiredFaviconSizeInPoints;

  switch (_selectedLayout) {
    case VivaldiStartPageLayoutStyleLarge:
    case VivaldiStartPageLayoutStyleMedium:
      desiredFaviconSizeInPoints = kDesiredSmallFaviconSizePt;
      break;
    case VivaldiStartPageLayoutStyleSmall:
    case VivaldiStartPageLayoutStyleList:
      desiredFaviconSizeInPoints = kDesiredMediumFaviconSizePt;
      break;
  }

  // Load direct match units and check if the URL matches any item.
  std::vector<const direct_match::DirectMatchService::DirectMatchUnit*> units =
      _directMatchService->GetPopularSites();

  UIImage* directMatchImage = nil;

  for (const auto* unit : units) {
    if (unit) {
      // Create GURL objects for comparison.
      GURL unitURL(unit->redirect_url);
      if (unitURL == item.url) {
        // URLs match, load image from the local path.
        NSString* imagePath =
            base::SysUTF8ToNSString(unit->image_path.c_str());
        UIImage* image = [UIImage imageWithContentsOfFile:imagePath];
        if (image) {
          directMatchImage = image;
        }
        break;  // Match found, no need to continue.
      }
    }
  }

  __weak VivaldiSpeedDialContainerView* weakSelf = self;
  auto faviconLoadedBlock = ^(FaviconAttributes* attributes) {
    VivaldiSpeedDialContainerView* strongSelf = weakSelf;
    if (!strongSelf)
      return;

    // Configure the cell based on the selected layout.
    switch (strongSelf.selectedLayout) {
      case VivaldiStartPageLayoutStyleLarge:
      case VivaldiStartPageLayoutStyleMedium: {
        VivaldiSpeedDialRegularCell* largeCell =
            (VivaldiSpeedDialRegularCell*)cell;
        [largeCell configureCellWithAttributes:attributes item:item];
        break;
      }
      case VivaldiStartPageLayoutStyleSmall: {
        VivaldiSpeedDialSmallCell* smallCell =
            (VivaldiSpeedDialSmallCell*)cell;
        [smallCell configureCellWithAttributes:attributes item:item];
        break;
      }
      case VivaldiStartPageLayoutStyleList: {
        VivaldiSpeedDialListCell* listCell =
            (VivaldiSpeedDialListCell*)cell;
        [listCell configureCellWithAttributes:attributes item:item];
        break;
      }
    }
  };

  if (directMatchImage) {
    // If a direct match image is found, create attributes and call the block.
    FaviconAttributes* attributes =
        [FaviconAttributes attributesWithImage:directMatchImage];
    faviconLoadedBlock(attributes);
  } else {
    // No direct match found, use the favicon loader.
    self.faviconLoader->FaviconForPageUrlOrHost(
        item.url, desiredFaviconSizeInPoints, faviconLoadedBlock);
  }
}

#pragma mark - COLLECTIONVIEW DELEGATE
- (void)collectionView:(UICollectionView *)collectionView
didSelectItemAtIndexPath:(NSIndexPath *)indexPath {
  // If last item tapped then open the view controller for adding new speed dial
  // item. Otherwise, pass the node which will be handleed by the delegate
  // method implementation.
  if (indexPath.row == (long)self.speedDialItems.count) {
    // Add speed dial item. The boolean in the method represents whether its
    // speed dial item or speed dial folder
    if (self.delegate)
        [self.delegate didSelectAddNewSpeedDial:NO
                                         parent:self.parent];
  } else {
    VivaldiSpeedDialItem* item =
      [self.speedDialItems objectAtIndex: indexPath.row];
    if (self.delegate)
        [self.delegate didSelectItem:item
                              parent:self.parent];
  }
}

#pragma mark - COLLECTIONVIEW DRAG DELEGATE

- (NSArray<UIDragItem *> *)collectionView:(UICollectionView *)collectionView
             itemsForBeginningDragSession:(id<UIDragSession>)session
                              atIndexPath:(NSIndexPath *)indexPath {
  // Disable drag and drop for top sites
  if (self.showingFrequentlyVisited)
    return @[];

  // If there's at least two items apart from the add speed dial tile then
  // enable the drag and drop otherwise return
  if (!(self.speedDialItems.count > 1))
    return @[];

  // Disable drag and drop for the add speed dial tile
  if (indexPath.row == (long)self.speedDialItems.count)
    return @[];

  VivaldiSpeedDialItem* item =
    [self.speedDialItems objectAtIndex: indexPath.row];
  NSItemProvider* itemProvider = [[NSItemProvider alloc] initWithObject:item];
  UIDragItem* dragItem = [[UIDragItem alloc] initWithItemProvider:itemProvider];
  dragItem.localObject = item;
  return @[dragItem];
}

#pragma mark - COLLECTIONVIEW DROP DELEGATE

- (UICollectionViewDropProposal*)
  collectionView:(UICollectionView*)collectionView
dropSessionDidUpdate:(id<UIDropSession>)session
withDestinationIndexPath:(NSIndexPath *)destinationIndexPath {

  if (collectionView.hasActiveDrag) {
    UICollectionViewDropProposal *proposal =
      [[UICollectionViewDropProposal alloc]
        initWithDropOperation:UIDropOperationMove
          intent:UICollectionViewDropIntentInsertAtDestinationIndexPath];
    return proposal;
  }

  UICollectionViewDropProposal *proposal =
    [[UICollectionViewDropProposal alloc]
     initWithDropOperation:UIDropOperationForbidden];
  return proposal;
}

- (void)collectionView:(UICollectionView *)collectionView
performDropWithCoordinator:(id<UICollectionViewDropCoordinator>)coordinator {
  NSIndexPath* destinationIndexPath;

  NSIndexPath* indexPath = coordinator.destinationIndexPath;
  if (indexPath) {
    destinationIndexPath = indexPath;
  } else {
    NSInteger row = [collectionView numberOfItemsInSection:0];
    destinationIndexPath = [NSIndexPath indexPathForRow:row inSection:0];
  }

  if (destinationIndexPath.row >= (long)self.speedDialItems.count)
    return;

  if (coordinator.proposal.operation == UIDropOperationMove) {
    [self reorderItems:coordinator
  destinationIndexPath:destinationIndexPath
        collectionView:collectionView];
  }
}

- (void)reorderItems:(id<UICollectionViewDropCoordinator>)coordinator
destinationIndexPath:(NSIndexPath*)destinationIndexPath
      collectionView:(UICollectionView*)collectionView {

  id<UICollectionViewDropItem> item = coordinator.items.firstObject;
  if (!item)
    return;
  NSIndexPath* sourceIndexPath = item.sourceIndexPath;
  if (!sourceIndexPath)
    return;
  VivaldiSpeedDialItem* dragItem = item.dragItem.localObject;
  [collectionView performBatchUpdates:^{
    [self.speedDialItems removeObjectAtIndex: sourceIndexPath.item];
    [self.speedDialItems insertObject:dragItem
                              atIndex:destinationIndexPath.item];
    [collectionView deleteItemsAtIndexPaths:@[sourceIndexPath]];
    [collectionView insertItemsAtIndexPaths:@[destinationIndexPath]];
  } completion:nil];

  [coordinator dropItem:item.dragItem toItemAtIndexPath:destinationIndexPath];

  NSInteger distance = destinationIndexPath.item - sourceIndexPath.item;
  NSInteger newPosition = distance > 0 ?
    (destinationIndexPath.item + 1) : destinationIndexPath.item;
  if (self.delegate)
    [self.delegate didMoveItemByDragging:dragItem
                                  parent:self.parent
                              toPosition:newPosition];
}

#pragma mark - DirectMatchServiceBridgeObserver

- (void)directMatchUnitsDownloaded {
  // No op.
}

- (void)directMatchUnitsIconDownloaded {
  [self.collectionView reloadData];
}

#pragma mark - CONTEXT MENU
- (UIContextMenuConfiguration*)collectionView:(UICollectionView *)collectionView
    contextMenuConfigurationForItemAtIndexPath:(NSIndexPath *)indexPath
                                         point:(CGPoint)point {
  // If the last item is tapped show the context menu for add new speed dial
  // item
  if (indexPath.row == (long)self.speedDialItems.count) {
    return [self contextMenuForAddNewItem];
  } else {
    return [self contextMenuForRegularItem: indexPath];
  }
}

/// Returns the context menu item for regular item such as a an speed dial
/// URL item or speed dial folder item.
- (UIContextMenuConfiguration*)contextMenuForRegularItem:
(NSIndexPath*)indexPath {
  UIContextMenuConfiguration* config =
  [UIContextMenuConfiguration configurationWithIdentifier:nil
                                          previewProvider:nil
                                           actionProvider:^UIMenu*
   _Nullable(NSArray<UIMenuElement*>* _Nonnull menuActions) {

    VivaldiSpeedDialItem* item =
        [self.speedDialItems objectAtIndex: indexPath.row];

    NSString* editActionTitle = l10n_util::GetNSString(IDS_IOS_EDIT_SPEED_DIAL);
    UIAction * editAction =
      [UIAction actionWithTitle:editActionTitle
                          image:[UIImage systemImageNamed:@"pencil"]
                     identifier:nil
                        handler:^(__kindof UIAction* _Nonnull action) {
        // Edit button action
        if (self.delegate) {
          [self.delegate didSelectEditItem:item
                                    parent:self.parent];
        }
      }];

    NSString* updateThumbnailActionTitle =
        l10n_util::GetNSString(IDS_IOS_UPDATE_SPEED_DIAL_THUMBNAIL);
    UIAction * thumbnailRefreshAction =
      [UIAction actionWithTitle:updateThumbnailActionTitle
                          image:[UIImage systemImageNamed:@"arrow.circlepath"]
                     identifier:nil
                        handler:^(__kindof UIAction* _Nonnull action) {
        // Thumbnail refresh button action
        if (self.delegate) {
          [self.delegate didRefreshThumbnailForItem:item
                                             parent:self.parent];
        }
      }];

    NSString* moveActionTitle = l10n_util::GetNSString(IDS_IOS_MOVE_ITEM);
    UIAction * moveAction =
      [UIAction actionWithTitle:moveActionTitle
                          image:[UIImage systemImageNamed:@"folder.badge.minus"]
                     identifier:nil
                        handler:^(__kindof UIAction* _Nonnull action) {
        // Move out of folder action
        if (self.delegate) {
          [self.delegate didSelectMoveItem:item
                                    parent:self.parent];
        }
      }];

    NSString* deleteActionTitle =
      l10n_util::GetNSString(IDS_IOS_SPEED_DIAL_ITEM_DELETE);
    UIAction * deleteAction =
      [UIAction actionWithTitle:deleteActionTitle
                          image:[UIImage systemImageNamed:@"trash"]
                     identifier:nil
                        handler:^(__kindof UIAction* _Nonnull action) {
        // Delete button action
        if (self.delegate) {
          [self.delegate didSelectDeleteItem:item
                                      parent:self.parent];
        }
      }];
    deleteAction.attributes = UIMenuElementAttributesDestructive;

    NSMutableArray *actions = [[NSMutableArray alloc] init];

    if (!item.isFolder) {
      // Add open in new tab item.
      UIAction* openAction =
          [self.actionFactory actionToOpenInNewTabWithBlock:^{
            if (self.delegate) {
              [self.delegate didSelectItemToOpenInNewTab:item
                                                  parent:self.parent];
            }
          }];
      [actions addObject:openAction];

      // Add open in background tab item.
      UIAction* openNewBackgroundTab =
        [self.actionFactory actionToOpenInNewBackgroundTabWithBlock:^{
          if (self.delegate) {
            [self.delegate didSelectItemToOpenInBackgroundTab:item
                                                       parent:self.parent];
          }
        }];
      [actions addObject:openNewBackgroundTab];

      // Add open in private menu item.
      UIAction* openInIncognito =
        [self.actionFactory actionToOpenInNewIncognitoTabWithBlock:^{
          if (self.delegate) {
            [self.delegate didSelectItemToOpenInPrivateTab:item
                                                    parent:self.parent];
          }
        }];
      [actions addObject:openInIncognito];

      // Add open URL in new window menu item.
      if (base::ios::IsMultipleScenesSupported()) {
        [actions addObject:
            [self.actionFactory actionToOpenInNewWindowWithURL:item.url
                    activityOrigin:WindowActivityBookmarksOrigin]];
      }

      // Copy link
      [actions addObject:[self.actionFactory
          actionToCopyURL:[[CrURL alloc] initWithGURL:item.url]]];

      // Share item
      [actions addObject:[self.actionFactory actionToShareWithBlock:^{
        if (self.delegate) {
          [self.delegate didSelectItemToShare:item
                                       parent:self.parent
                                     fromView:[self.collectionView
                                              cellForItemAtIndexPath:indexPath]];
        }
      }]];
    }

    if (self.showingFrequentlyVisited) {
      [actions addObject:deleteAction];
    } else {
      // Refresh is not available when thumbnail url is a synced-store thumbnail
      // or if the selected layout is anything other than regular or medium.
      BOOL layoutHasThumbnail =
          self.selectedLayout == VivaldiStartPageLayoutStyleLarge ||
          self.selectedLayout == VivaldiStartPageLayoutStyleMedium;

      BOOL shouldAddThumbnailRefreshAction =
          !item.isFolder &&
          layoutHasThumbnail &&
          ![self isSyncedStoreThumbnailURLForItem:item];

      if (shouldAddThumbnailRefreshAction) {
        [actions addObject:thumbnailRefreshAction];
      }

      [actions addObjectsFromArray:@[editAction, moveAction, deleteAction]];
    }

    // Create the menu with the actions array
    UIMenu* menu = [UIMenu menuWithTitle:@"" children:actions];
    return menu;
  }];

  return config;
}

/// Returns the context menu item for new speed dial item
- (UIContextMenuConfiguration*)contextMenuForAddNewItem {
  UIContextMenuConfiguration* config =
  [UIContextMenuConfiguration configurationWithIdentifier:nil
                                          previewProvider:nil
                                           actionProvider:^UIMenu*
   _Nullable(NSArray<UIMenuElement*>* _Nonnull menuActions) {
    NSString* newSpeedDialActionTitle = GetNSString(IDS_IOS_NEW_SPEED_DIAL);
    UIAction * newSpeedDialAction =
      [UIAction actionWithTitle:newSpeedDialActionTitle
                          image:[UIImage systemImageNamed:@"plus"]
                     identifier:nil
                        handler:^(__kindof UIAction* _Nonnull action) {
        // Add new speed dial action
        if (self.delegate)
          [self.delegate didSelectAddNewSpeedDial:NO
                                           parent:self.parent];
      }];

    NSString* newFolderActionTitle = GetNSString(IDS_IOS_NEW_FOLDER);
    UIAction * newFolderAction =
      [UIAction actionWithTitle:newFolderActionTitle
                          image:[UIImage systemImageNamed:@"folder.badge.plus"]
                     identifier:nil
                        handler:^(__kindof UIAction* _Nonnull action) {
        // Add new speed dial folder action
        if (self.delegate)
          [self.delegate didSelectAddNewSpeedDial:YES
                                           parent:self.parent];
    }];

    UIMenu* menu = [UIMenu menuWithTitle:@"" children:@[
      newSpeedDialAction, newFolderAction
    ]];
    return menu;

  }];

  return config;
}

#pragma mark - UIGestureRecognizerDelegate and gesture handling
- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)gestureRecognizer {
  // Only handle tapping empty space (i.e. not a cell)
  CGPoint point = [gestureRecognizer locationInView:self.collectionView];
  NSIndexPath *indexPath = [self.collectionView indexPathForItemAtPoint:point];
  return indexPath == nil;
}

#pragma mark - Private

- (void)setUpTapGesture {
  UITapGestureRecognizer *tapGestureCV =
      [[UITapGestureRecognizer alloc]
          initWithTarget:self
              action:@selector(handleTapGesture:)];
  tapGestureCV.delegate = self;
  [self.collectionView addGestureRecognizer:tapGestureCV];
}

- (void)handleTapGesture:(UITapGestureRecognizer*)gesture {
  if (self.delegate)
    [self.delegate didTapOnCollectionViewEmptyArea];
}

- (void)startObservingSDItemPropertyChange {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [[NSNotificationCenter defaultCenter]
       addObserver:self
          selector:@selector(handleSDItemPropertyChangeNotification:)
              name:vSpeedDialPropertyDidChange
            object:nil];
}

- (void)handleSDItemPropertyChangeNotification:(NSNotification*)notification {
  if (self.showingFrequentlyVisited)
    return;
  NSDictionary *userInfo = notification.userInfo;
  NSNumber *bookmarkNodeId = userInfo[vSpeedDialIdentifierKey];

  // Find the index of the SDItem with the matching id
  NSUInteger index =
      [_speedDialItems indexOfObjectPassingTest:^BOOL(VivaldiSpeedDialItem *item,
                                                      NSUInteger idx,
                                                      BOOL *stop) {
    return [item.idValue isEqualToNumber:bookmarkNodeId];
  }];

  if (index != NSNotFound) {
    // Create the NSIndexPath for the specific item and get the cell.
    NSIndexPath *indexPath = [NSIndexPath indexPathForItem:index inSection:0];
    NSNumber* triggeredFromThumbnailRefresh =
        userInfo[vSpeedDialThumbnailRefreshStateKey];
    if (triggeredFromThumbnailRefresh) {
      VivaldiSpeedDialItem* item = [self.speedDialItems objectAtIndex:index];
      BOOL isCaptureInProgress = ![triggeredFromThumbnailRefresh boolValue];
      if (item) {
        item.isThumbnailRefreshing = isCaptureInProgress;
      }
    }
    [_collectionView reloadItemsAtIndexPaths:@[indexPath]];
  }
}

/// Whether its a custom thumbnail set on Desktop.
- (BOOL)isSyncedStoreThumbnailURLForItem:(VivaldiSpeedDialItem*)item {
  if (!item)
    return NO;
  NSRange range = [item.thumbnail rangeOfString:syncedStoreURLKey];
  return range.location != NSNotFound;
}

/// Returns whether items should have tablet or phone layout.
- (BOOL)showTabletLayout {
  return [VivaldiGlobalHelpers canShowSidePanelForTrait:self.traitCollection];
}

@end
