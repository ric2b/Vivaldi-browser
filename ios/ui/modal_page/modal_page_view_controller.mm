// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/modal_page/modal_page_view_controller.h"

#import <WebKit/WebKit.h>

#import "base/check.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/modal_page/modal_page_commands.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ModalPageViewController ()

@property(nonatomic, strong) UIView* modalPageView;
@property(nonatomic, weak) id<ModalPageCommands> handler;

@end

@implementation ModalPageViewController

- (instancetype)initWithContentView:(UIView*)modalPageView
                            handler:(id<ModalPageCommands>)handler
                              title:(NSString*)title {
  DCHECK(modalPageView);
  self = [super init];
  if (self) {
    _modalPageView = modalPageView;
    _handler = handler;
    self.title = title;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self configureNavigationBar];
  self.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  [self.view addSubview:self.modalPageView];

  [self.modalPageView fillSuperview];
}

#pragma mark - Button events

// Called by the Done button from the navigation bar.
- (void)close {
  [self.handler closeModalPage];
}

#pragma mark - Private

// Configures the top navigation bar (colors, texts, buttons, etc.)
- (void)configureNavigationBar {
  self.navigationController.navigationBar.translucent = NO;
  self.navigationController.navigationBar.barTintColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  self.navigationController.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];

  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(close)];
  self.navigationItem.rightBarButtonItem = doneButton;
}

@end
