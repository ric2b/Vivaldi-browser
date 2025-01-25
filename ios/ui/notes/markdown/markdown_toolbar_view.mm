// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/notes/markdown/markdown_toolbar_view.h"

#import "base/check.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/notes/markdown/markdown_command_delegate.h"

namespace {

// Default Height for the toolbar.
constexpr CGFloat kDefaultToolbarHeight = 44;

}  // namespace

@interface MarkdownToolbarView () {
}

@property(nonatomic, strong) UIBarButtonItem* undoButton;
@property(nonatomic, strong) UIBarButtonItem* redoButton;
@property(nonatomic, strong) UIBarButtonItem* closeButton;
@property(nonatomic, strong) UIBarButtonItem* textFormatButton;
@property(nonatomic, strong) UIBarButtonItem* closeKeyboardButton;
@property(nonatomic, strong) UIBarButtonItem* flexibleSpace;

@end

@implementation MarkdownToolbarView

@synthesize commandDelegate = _commandDelegate;
@synthesize flexibleSpace = _flexibleSpace;
@synthesize leadGroup = _leadGroup;
@synthesize trailGroup = _trailGroup;

#pragma mark - INITIALIZER

- (instancetype)initWithFrame:(CGRect)frame
              commandDelegate:(id<MarkdownCommandDelegate>)commandDelegate {
  self = [super
      initWithFrame:CGRectMake(0, 0, [UIScreen mainScreen].bounds.size.width,
                               kDefaultToolbarHeight)];
  if (self) {
    CHECK(commandDelegate);
    _commandDelegate = commandDelegate;
    [self setUpUI];
  }
  return self;
}

- (void)dealloc {
  _commandDelegate = nil;
}

#pragma mark - SET UP UI COMPONENTS

- (void)setUpUI {
  if ([VivaldiGlobalHelpers isDeviceTablet]) {
    self.trailGroup = [[UIBarButtonItemGroup alloc]
        initWithBarButtonItems:@[ self.closeKeyboardButton ]
            representativeItem:nil];

    self.leadGroup = [[UIBarButtonItemGroup alloc] initWithBarButtonItems:@[
      self.undoButton, self.redoButton, self.textFormatButton
    ]
                                                       representativeItem:nil];
  } else {
    self.barStyle = UIBarStyleDefault;
    self.flexibleSpace = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                             target:nil
                             action:nil];
    [self setItems:@[
      self.undoButton, self.redoButton, self.flexibleSpace,
      self.textFormatButton, self.flexibleSpace, self.closeKeyboardButton
    ]];
  }
}

- (void)openMarkdownInputView:(UIBarButtonItem*)sender {
  if ([VivaldiGlobalHelpers isDeviceTablet]) {
    self.leadGroup = [[UIBarButtonItemGroup alloc] initWithBarButtonItems:@[
      self.undoButton, self.redoButton, self.closeButton
    ]
                                                       representativeItem:nil];
  } else {
    [self setItems:@[
      self.undoButton, self.redoButton, self.flexibleSpace, self.closeButton,
      self.flexibleSpace, self.closeKeyboardButton
    ]
          animated:YES];
  }
  [self.markdownDelegate openMarkdownInputView];
}

- (void)closeMarkdownInputView:(UIBarButtonItem*)sender {
  if ([VivaldiGlobalHelpers isDeviceTablet]) {
    self.leadGroup = [[UIBarButtonItemGroup alloc] initWithBarButtonItems:@[
      self.undoButton, self.redoButton, self.textFormatButton
    ]
                                                       representativeItem:nil];
  } else {
    [self setItems:@[
      self.undoButton, self.redoButton, self.flexibleSpace,
      self.textFormatButton, self.flexibleSpace, self.closeKeyboardButton
    ]
          animated:YES];
  }
  [self.markdownDelegate closeMarkdownInputView];
}

- (void)closeKeyboard:(UIBarButtonItem*)sender {
  [self.markdownDelegate closeKeyboard];
}

- (void)toolbarUndoAction:(UIBarButtonItem*)sender {
  [self.commandDelegate sendCommand:@""
                        commandType:@"UNDO_COMMAND"];
}
- (void)toolbarRedoAction:(UIBarButtonItem*)sender {
  [self.commandDelegate sendCommand:@""
                        commandType:@"REDO_COMMAND"];
}

#pragma mark - Toolbar button definitions

- (UIBarButtonItem*)createToolbarButtonItem:(NSString*)name
                                     action:(SEL)selector {
  UIImage* icon = [[UIImage systemImageNamed:name]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];

  UIBarButtonItem* button =
      [[UIBarButtonItem alloc] initWithImage:icon
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:selector];
  button.tintColor = [UIColor colorNamed:kTextPrimaryColor];

  return button;
}

- (UIBarButtonItem*)closeKeyboardButton {
  if (!_closeKeyboardButton) {
    _closeKeyboardButton =
        [self createToolbarButtonItem:@"keyboard.chevron.compact.down"
                               action:@selector(closeKeyboard:)];
  }
  return _closeKeyboardButton;
}

- (UIBarButtonItem*)redoButton {
  if (!_redoButton) {
    _redoButton = [self createToolbarButtonItem:@"arrow.uturn.forward"
                                         action:@selector(toolbarRedoAction:)];
  }
  return _redoButton;
}

- (UIBarButtonItem*)undoButton {
  if (!_undoButton) {
    _undoButton = [self createToolbarButtonItem:@"arrow.uturn.backward"
                                         action:@selector(toolbarUndoAction:)];
  }
  return _undoButton;
}

- (UIBarButtonItem*)closeButton {
  if (!_closeButton) {
    _closeButton =
        [self createToolbarButtonItem:@"xmark"
                               action:@selector(closeMarkdownInputView:)];
  }
  return _closeButton;
}

- (UIBarButtonItem*)textFormatButton {
  if (!_textFormatButton) {
    _textFormatButton =
        [self createToolbarButtonItem:@"bold.italic.underline"
                               action:@selector(openMarkdownInputView:)];
  }
  return _textFormatButton;
}

@end
