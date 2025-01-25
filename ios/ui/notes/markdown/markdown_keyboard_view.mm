// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/notes/markdown/markdown_keyboard_view.h"

#import "base/check.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/notes/markdown/markdown_command_delegate.h"

namespace {

typedef NS_ENUM(NSInteger, CommandId) {
  Bold = 0,
  Italic,
  Underline,
  Strikethrough,
  Indent,
  Outdent,
  OrderedList,
  UnorderedList,
  CheckList,
  Link,
  AlignCenter,
  AlignRight,
  AlignLeft,
  AlignJustify,
  Highlight,
  Code,
  Subscript,
  Superscript,
  Heading1,
  Heading2,
  Heading3,
  Heading4,
  Paragraph,
  CodeBlock,
  Quote,
  Image,

  BlankSpace,
};

// Padding around the buttons in the keyboard view
constexpr CGFloat kDefaultPadding = 60;
constexpr CGFloat kPageControlBottomPadding = -15;
constexpr NSInteger kNumberOfKeyboardPages = 2;

}  // namespace

@interface MarkdownKeyboardView () <UIScrollViewDelegate>

@property(nonatomic, strong) UIScrollView* scrollView;
@property(nonatomic, strong) UIPageControl* pageControl;

@end

@implementation MarkdownKeyboardView

@synthesize commandDelegate = _commandDelegate;

#pragma mark - INITIALIZER

- (instancetype)initWithFrame:(CGRect)frame
              commandDelegate:(id<MarkdownCommandDelegate>)commandDelegate {
  self = [super initWithFrame:frame];
  if (self) {
    CHECK(commandDelegate);
    _commandDelegate = commandDelegate;
    if ([VivaldiGlobalHelpers isDeviceTablet]) {
      [self setUpTabletUI];
    } else {
      [self setUpPhoneUI];
    }
  }
  return self;
}

- (void)dealloc {
  _commandDelegate = nil;
  self.scrollView.delegate = nil;
  self.scrollView = nil;
  self.pageControl = nil;
}

#pragma mark - SET UP UI COMPONENTS

- (void)setUpTabletUI {
  self.backgroundColor = [UIColor colorNamed:kGrey300Color];
  NSArray* buttonLayout = @[
    [self getButtonGroup:@[
      @(Bold),
      @(Italic),
      @(Underline), // TODO(tomas@vivaldi): does not get saved
      @(Strikethrough),
      @(Subscript),
      @(Superscript),
    ]],
    [self getButtonGroup:@[
      @(Heading1),  // block type
      @(Heading2),  // block type
      @(Heading3),  // block type
      @(Heading4),  // block type
      @(Paragraph), // block type
      @(CodeBlock), // block type
    ]],
    [self getButtonGroup:@[
      @(Image),
      @(Link),
      @(AlignLeft),    // TODO(tomas@vivaldi): does not get saved
      @(AlignCenter),  // TODO(tomas@vivaldi): does not get saved
      @(AlignRight),   // TODO(tomas@vivaldi): does not get saved
      @(Code),
    ]],
    [self getButtonGroup:@[
      @(OrderedList),
      @(UnorderedList),
      @(CheckList),
      @(Indent),   // TODO(tomas@vivaldi): does not get saved
      @(Outdent),  // TODO(tomas@vivaldi): does not get saved
      @(Highlight),
    ]],
    [self getButtonGroup:@[
      @(Quote),
      @(BlankSpace),
      @(BlankSpace),
      @(BlankSpace),
      @(BlankSpace),
      @(BlankSpace),
    ]],
  ];

  UIStackView* keysLayout = [[UIStackView alloc]
      initWithArrangedSubviews:[self stackWithArrangedSubviews:buttonLayout]];
  keysLayout.axis = UILayoutConstraintAxisVertical;
  keysLayout.translatesAutoresizingMaskIntoConstraints = NO;
  keysLayout.distribution = UIStackViewDistributionEqualSpacing;

  UIView* containerView = [[UIView alloc] init];
  containerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:containerView];
  [containerView
      fillSuperviewToSafeAreaInsetWithPadding:UIEdgeInsetsMake(
                                                  kDefaultPadding,
                                                  kDefaultPadding,
                                                  kDefaultPadding/2,
                                                  kDefaultPadding)];
  [containerView addSubview:keysLayout];

  [NSLayoutConstraint activateConstraints:@[
    [keysLayout.topAnchor constraintEqualToAnchor:containerView.topAnchor],
    [keysLayout.leadingAnchor
        constraintEqualToAnchor:containerView.leadingAnchor],
    [keysLayout.bottomAnchor
        constraintEqualToAnchor:containerView.bottomAnchor],
    [keysLayout.widthAnchor
        constraintEqualToAnchor:containerView.widthAnchor]
  ]];
}

- (void)setUpPhoneUI {
  self.backgroundColor = [UIColor colorNamed:kGrey300Color];
  NSArray* buttonLayout1 = @[
    [self getButtonGroup:@[
      @(Bold),
      @(Italic),
      @(Underline), // TODO(tomas@vivaldi): does not get saved
      @(Strikethrough),
    ]],
    [self getButtonGroup:@[
      @(Heading1),  // block type
      @(Heading2),  // block type
      @(Paragraph), // block type
      @(Code),
    ]],
    [self getButtonGroup:@[
      @(AlignLeft),    // TODO(tomas@vivaldi): does not get saved
      @(AlignCenter),  // TODO(tomas@vivaldi): does not get saved
      @(AlignRight),   // TODO(tomas@vivaldi): does not get saved
      @(Highlight),
    ]],
    [self getButtonGroup:@[
      @(OrderedList),
      @(UnorderedList),
      @(Indent),   // TODO(tomas@vivaldi): does not get saved
      @(Outdent),  // TODO(tomas@vivaldi): does not get saved
    ]],
  ];

  NSArray* buttonLayout2 = @[
    [self getButtonGroup:@[
      @(Quote),
      @(CheckList),
      @(Link),
      @(CodeBlock),    // block type
    ]],
    [self getButtonGroup:@[
      @(Heading3),     // block type
      @(Heading4),     // block type
      @(Subscript),
      @(Superscript),
    ]],
    [self getButtonGroup:@[
      @(Image),
      @(BlankSpace),
      @(BlankSpace),
      @(BlankSpace),
    ]],
    [self getButtonGroup:@[
      @(BlankSpace),
      @(BlankSpace),
      @(BlankSpace),
      @(BlankSpace),
    ]],
  ];

  UIStackView* keysPageOne = [[UIStackView alloc]
      initWithArrangedSubviews:[self stackWithArrangedSubviews:buttonLayout1]];
  keysPageOne.axis = UILayoutConstraintAxisVertical;
  keysPageOne.translatesAutoresizingMaskIntoConstraints = NO;
  keysPageOne.distribution = UIStackViewDistributionEqualSpacing;

  UIStackView* keysPageTwo = [[UIStackView alloc]
      initWithArrangedSubviews:[self stackWithArrangedSubviews:buttonLayout2]];
  keysPageTwo.axis = UILayoutConstraintAxisVertical;
  keysPageTwo.translatesAutoresizingMaskIntoConstraints = NO;
  keysPageTwo.distribution = UIStackViewDistributionEqualSpacing;

  self.scrollView = [[UIScrollView alloc] init];
  self.scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  self.scrollView.pagingEnabled = YES;
  self.scrollView.showsHorizontalScrollIndicator = NO;
  self.scrollView.alwaysBounceHorizontal = YES;
  self.scrollView.delegate = self;

  [self addSubview:self.scrollView];

  [self.scrollView
      fillSuperviewToSafeAreaInsetWithPadding:UIEdgeInsetsMake(
                                                  kDefaultPadding,
                                                  kDefaultPadding,
                                                  kDefaultPadding/2,
                                                  kDefaultPadding)];

  UIView* containerView = [[UIView alloc] init];
  containerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.scrollView addSubview:containerView];

  [NSLayoutConstraint activateConstraints:@[
    [containerView.topAnchor constraintEqualToAnchor:self.scrollView.topAnchor],
    [containerView.leadingAnchor
        constraintEqualToAnchor:self.scrollView.leadingAnchor],
    [containerView.trailingAnchor
        constraintEqualToAnchor:self.scrollView.trailingAnchor],
    [containerView.bottomAnchor
        constraintEqualToAnchor:self.scrollView.bottomAnchor],
    [containerView.heightAnchor
        constraintEqualToAnchor:self.scrollView.heightAnchor]
  ]];

  [containerView addSubview:keysPageOne];
  [containerView addSubview:keysPageTwo];

  [NSLayoutConstraint activateConstraints:@[
    [keysPageOne.topAnchor constraintEqualToAnchor:containerView.topAnchor],
    [keysPageOne.leadingAnchor
        constraintEqualToAnchor:containerView.leadingAnchor],
    [keysPageOne.bottomAnchor
        constraintEqualToAnchor:containerView.bottomAnchor],
    [keysPageOne.widthAnchor
        constraintEqualToAnchor:self.scrollView.widthAnchor]
  ]];
  [NSLayoutConstraint activateConstraints:@[
    [keysPageTwo.topAnchor constraintEqualToAnchor:containerView.topAnchor],
    [keysPageTwo.leadingAnchor
        constraintEqualToAnchor:keysPageOne.trailingAnchor],
    [keysPageTwo.bottomAnchor
        constraintEqualToAnchor:containerView.bottomAnchor],
    [keysPageTwo.widthAnchor
        constraintEqualToAnchor:self.scrollView.widthAnchor]
  ]];

  // Set the content size of the scroll view based on two pages (stack views)
  [NSLayoutConstraint activateConstraints:@[
    [containerView.trailingAnchor
        constraintEqualToAnchor:keysPageTwo.trailingAnchor]
  ]];

  self.pageControl = [[UIPageControl alloc] init];
  self.pageControl.translatesAutoresizingMaskIntoConstraints = NO;
  self.pageControl.numberOfPages = kNumberOfKeyboardPages;
  self.pageControl.currentPage = 0;
  self.pageControl.currentPageIndicatorTintColor =
      [UIColor colorNamed:kStaticGrey900Color];
  self.pageControl.pageIndicatorTintColor = [UIColor colorNamed:kGrey600Color];
  [self.pageControl addTarget:self
                       action:@selector(pageControlChanged:)
             forControlEvents:UIControlEventValueChanged];

  [self addSubview:self.pageControl];

  CGSize pageControlSize = [self.pageControl sizeThatFits:self.bounds.size];

  [NSLayoutConstraint activateConstraints:@[
    [self.pageControl.bottomAnchor
        constraintEqualToAnchor:self.bottomAnchor
                       constant:kPageControlBottomPadding],
    [self.pageControl.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
    [self.pageControl.heightAnchor
        constraintEqualToConstant:pageControlSize.height]
  ]];
}

- (NSArray<UIView*>*)stackWithArrangedSubviews:(NSArray*)buttongroups {
  NSMutableArray* group = [[NSMutableArray alloc] init];
  for (NSUInteger i = 0; i < [buttongroups count]; i++) {
    [group addObject:[self createHorizontalStackView:buttongroups[i]]];
  }
  return group;
}

- (UIStackView*)createHorizontalStackView:(NSArray<UIButton*>*)buttons {
  UIStackView* stackView =
      [[UIStackView alloc] initWithArrangedSubviews:buttons];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.distribution = UIStackViewDistributionEqualSpacing;
  return stackView;
}

- (UIButton*)buttonWithImage:(NSInteger)commandId {
  UIImage* icon = [[self getIconForCommand:commandId]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  UIButton* button = [UIButton systemButtonWithImage:icon
                                              target:self
                                              action:@selector(sendCommand:)];
  button.tintColor = [UIColor colorNamed:kTextPrimaryColor];
  [button setTag:commandId];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  return button;
}

- (NSArray<UIButton*>*)getButtonGroup:(NSArray*)commandIds {
  NSMutableArray* group = [[NSMutableArray alloc] init];
  for (NSUInteger i = 0; i < [commandIds count]; i++) {
    [group addObject:[self buttonWithImage:[commandIds[i] integerValue]]];
  }
  return group;
}

- (void)sendCommand:(UIButton*)sender {
  if (sender.tag == BlankSpace) {
    return;
  }
  [self.commandDelegate sendCommand:[self getCommandName:sender.tag]
                        commandType:[self getCommandType:sender.tag]];
}

#pragma mark - Command Helper Functions

- (UIImage*)getIconForCommand:(NSInteger)commandId {
  switch (commandId) {
    case Bold:
      return [UIImage systemImageNamed:@"bold"];
    case Italic:
      return [UIImage systemImageNamed:@"italic"];
    case Underline:
      return [UIImage systemImageNamed:@"underline"];
    case Strikethrough:
      return [UIImage systemImageNamed:@"strikethrough"];
    case Highlight:
      return [UIImage systemImageNamed:@"highlighter"];
    case Code:
      return [UIImage imageNamed:@"markdown_code_button"];
    case CodeBlock:
      return [UIImage imageNamed:@"markdown_code_block_button"];
    case Subscript:
      return [UIImage systemImageNamed:@"textformat.subscript"];
    case Superscript:
      return [UIImage systemImageNamed:@"textformat.superscript"];
    case AlignCenter:
      return [UIImage systemImageNamed:@"text.aligncenter"];
    case AlignRight:
      return [UIImage systemImageNamed:@"text.alignright"];
    case AlignLeft:
      return [UIImage systemImageNamed:@"text.alignleft"];
    case AlignJustify:
      return [UIImage systemImageNamed:@"text.justify"];
    case Heading1:
      return [UIImage imageNamed:@"markdown_h1_button"];
    case Heading2:
      return [UIImage imageNamed:@"markdown_h2_button"];
    case Heading3:
      return [UIImage imageNamed:@"markdown_h3_button"];
    case Heading4:
      return [UIImage imageNamed:@"markdown_h4_button"];
    case Paragraph:
      return [UIImage systemImageNamed:@"paragraph"];
    case Quote:
      return [UIImage systemImageNamed:@"text.quote"];
    case Indent:
      return [UIImage systemImageNamed:@"increase.indent"];
    case Outdent:
      return [UIImage systemImageNamed:@"decrease.indent"];
    case OrderedList:
      return [UIImage systemImageNamed:@"list.number"];
    case UnorderedList:
      return [UIImage systemImageNamed:@"list.bullet"];
    case CheckList:
      return [UIImage systemImageNamed:@"checklist"];
    case Link:
      return [UIImage systemImageNamed:@"link"];
    case Image:
      return [UIImage systemImageNamed:@"photo"];
    case BlankSpace:
      return nil;

    default:
      return nil;
  }
}

// NOTE(tomas@vivaldi): These must match the lexical definitions
// See usage in vivapp/src/mobile/markdown/ToolbarHandler.jsx
- (NSString*)getCommandName:(NSInteger)commandId {
  switch (commandId) {
    case Bold:
      return @"bold";
    case Italic:
      return @"italic";
    case Underline:
      return @"underline";
    case Strikethrough:
      return @"strikethrough";
    case Highlight:
      return @"highlight";
    case Code:
    case CodeBlock:
      return @"code";
    case Subscript:
      return @"subscript";
    case Superscript:
      return @"superscript";
    case AlignCenter:
      return @"center";
    case AlignRight:
      return @"right";
    case AlignLeft:
      return @"left";
    case AlignJustify:
      return @"justify";
    case Heading1:
      return @"h1";
    case Heading2:
      return @"h2";
    case Heading3:
      return @"h3";
    case Heading4:
      return @"h4";
    case Paragraph:
      return @"paragraph";
    case Quote:
      return @"quote";
    case Indent:
    case Outdent:
    case OrderedList:
    case UnorderedList:
    case CheckList:
    case Link:
    case Image:
    case BlankSpace:
      return @"";

    default:
      return @"error";
  }
}

// NOTE(tomas@vivaldi): These must match the lexical definitions
// See usage in vivapp/src/mobile/markdown/ToolbarHandler.jsx
- (NSString*)getCommandType:(NSInteger)tag {
  switch (tag) {
    case Bold:
    case Italic:
    case Underline:
    case Strikethrough:
    case Highlight:
    case Code:
    case Subscript:
    case Superscript:
      return @"FORMAT_TEXT_COMMAND";
    case Indent:
      return @"INDENT_CONTENT_COMMAND";
    case Outdent:
      return @"OUTDENT_CONTENT_COMMAND";
    case OrderedList:
      return @"INSERT_ORDERED_LIST_COMMAND";
    case UnorderedList:
      return @"INSERT_UNORDERED_LIST_COMMAND";
    case CheckList:
      return @"INSERT_CHECK_LIST_COMMAND";
    case Link:
      return @"TOGGLE_LINK_COMMAND";
    case AlignCenter:
    case AlignRight:
    case AlignLeft:
    case AlignJustify:
      return @"FORMAT_ELEMENT_COMMAND";
    case Heading1:
    case Heading2:
    case Heading3:
    case Heading4:
    case Paragraph:
    case CodeBlock:
    case Quote:
      return @"BLOCK_TYPE_COMMAND";
    case Image:
      return @"INSERT_IMAGE_COMMAND";

    default:
      return @"";
  }
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView {
  CGFloat pageWidth = scrollView.frame.size.width;
  NSInteger page = (NSInteger)(scrollView.contentOffset.x / pageWidth);
  self.pageControl.currentPage = page;
}

- (void)pageControlChanged:(UIPageControl*)sender {
  CGFloat pageWidth = self.scrollView.frame.size.width;
  CGPoint offset = CGPointMake(sender.currentPage * pageWidth, 0);
  [self.scrollView setContentOffset:offset animated:YES];
}

@end