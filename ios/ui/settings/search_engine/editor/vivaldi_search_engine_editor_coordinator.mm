// Copyright 2025 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/search_engine/editor/vivaldi_search_engine_editor_coordinator.h"

#import "components/search_engines/template_url.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/ui/helpers/helpers_swift.h"
#import "ios/ui/settings/search_engine/editor/vivaldi_search_engine_editor_mediator.h"
#import "ios/ui/settings/search_engine/editor/vivaldi_search_engine_editor_swift.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

@interface VivaldiSearchEngineEditorCoordinator () <
    VivaldiHostingControllerPresentationDelegate> {
  // Template URL being edited (null for new search engines)
  const TemplateURL* _editingItem;
}

// SwiftUI view provider for the search engine editor interface
@property(nonatomic, strong)
    VivaldiSearchEngineEditorViewProvider* viewProvider;

// Main view controller hosting the SwiftUI interface
@property(nonatomic, strong) UIViewController* controller;

// Mediator handling business logic and data management
@property(nonatomic, strong) VivaldiSearchEngineEditorMediator* mediator;

// Whether we're editing an existing engine (YES) or creating new (NO)
@property(nonatomic, assign) BOOL isEditing;

// Whether the user has made unsaved changes
@property(nonatomic, assign) BOOL hasActiveChanges;

// Whether the edited form data is valid and can be saved
@property(nonatomic, assign) BOOL isFormDataValid;

@end

@implementation VivaldiSearchEngineEditorCoordinator

@synthesize baseNavigationController = _baseNavigationController;

#pragma mark - Initialization

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                       isEditing:(BOOL)isEditing
                                     editingItem:
                                         (const TemplateURL*)editingItem {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _isEditing = isEditing;
    _editingItem = editingItem;
    _hasActiveChanges = NO;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [self setupViewProvider];
  [self setupViewController];
  [self setupMediator];
  [self setupNavigationItems];
  [self presentViewController];
}

- (void)stop {
  [super stop];
  [self cleanup];
}

#pragma mark - Private Setup Methods

- (void)setupViewProvider {
  self.viewProvider = [[VivaldiSearchEngineEditorViewProvider alloc] init];

  __weak __typeof(self) weakSelf = self;

  // Observe item state changes to show save button
  [self.viewProvider
      observeItemStateChangeEvent:^(VivaldiSearchEngineEditorItem* item) {
        [weakSelf handleItemStateChange:item];
      }];

  // Observe backend changes to dismiss editor
  [self.viewProvider observeSearchEngineBackendDidChangeEvent:^{
    [weakSelf handleBackendChange];
  }];
}

- (void)setupViewController {
  self.controller =
      [self.viewProvider makeViewControllerWithPresentationDelegate:self];

  NSString* title =
      self.isEditing ? l10n_util::GetNSString(
                           IDS_VIVALDI_SEARCH_ENGINE_EDITOR_EDIT_ENGINE_TITLE)
                     : l10n_util::GetNSString(
                           IDS_VIVALDI_SEARCH_ENGINE_EDITOR_ADD_ENGINE_TITLE);

  self.controller.title = title;
  self.controller.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
}

- (void)setupMediator {
  self.mediator = [[VivaldiSearchEngineEditorMediator alloc]
      initWithProfile:self.browser->GetProfile()
            isEditing:self.isEditing
          editingItem:_editingItem];

  // Connect mediator and view provider
  self.mediator.consumer = self.viewProvider;
  self.viewProvider.consumer = self.mediator;
}

- (void)setupNavigationItems {
  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(handleDoneButtonTap)];

  self.controller.navigationItem.rightBarButtonItem = doneButton;
}

- (void)presentViewController {
  [self.baseNavigationController pushViewController:self.controller
                                           animated:YES];
}

#pragma mark - Event Handlers

- (void)handleItemStateChange:(VivaldiSearchEngineEditorItem*)item {
  self.isFormDataValid = item.isFormValid;
  if (self.hasActiveChanges) {
    return;
  }

  self.hasActiveChanges = YES;

  UIBarButtonItem* saveButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemSave
                           target:self
                           action:@selector(handleSaveButtonTap)];

  self.controller.navigationItem.rightBarButtonItem = saveButton;
}

- (void)handleBackendChange {
  [self.delegate searchEngineEditorShouldDismiss:self];
  [self.baseNavigationController popViewControllerAnimated:YES];
}

#pragma mark - Button Actions

- (void)handleDoneButtonTap {
  [self stop];
  [self.baseNavigationController dismissViewControllerAnimated:YES
                                                    completion:nil];
}

- (void)handleSaveButtonTap {
  if (!self.isFormDataValid)
    return;
  [self.mediator saveChanges];
}

#pragma mark - Cleanup

- (void)cleanup {
  _editingItem = nil;
  self.viewProvider.consumer = nil;
  self.viewProvider = nil;
  self.controller = nil;

  [self.mediator disconnect];
  self.mediator = nil;

  self.hasActiveChanges = NO;
}

#pragma mark - VivaldiHostingControllerPresentationDelegate

- (void)hostingController:(UIViewController*)hostingController
                didMoveTo:(UIViewController*)parent {
  DCHECK_EQ(self.controller, hostingController);
  [self.delegate searchEngineEditorShouldDismiss:self];
}

@end
