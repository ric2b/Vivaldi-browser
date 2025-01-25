// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/onboarding/vivaldi_onboarding_agreement_view.h"

#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ios/ui/common/vivaldi_url_constants.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

@implementation VivaldiOnboardingAgreementView

#pragma mark - Initializers

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    // Setup the view
    [self setupView];
  }
  return self;
}

#pragma mark - Setup Methods

// Setup the view by adding the textView and configuring its attributed text.
- (void)setupView {
  self.backgroundColor = [UIColor clearColor];

  _textView = CreateUITextViewWithTextKit1();
  _textView.textColor = [UIColor secondaryLabelColor];
  _textView.scrollEnabled = NO;
  _textView.editable = NO;
  _textView.backgroundColor = [UIColor clearColor];
  _textView.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  _textView.textAlignment = NSTextAlignmentCenter;
  _textView.textContainerInset = UIEdgeInsetsZero;
  _textView.linkTextAttributes =
      @{NSForegroundColorAttributeName : [UIColor secondaryLabelColor],
        NSUnderlineStyleAttributeName : @(NSUnderlineStyleSingle)};

  _textView.attributedText = self.createAttributedText;
  _textView.delegate = self;

  [self addSubview:_textView];
  [_textView fillSuperview];
}

#pragma mark - Attributed Text Creation

// Creates and returns the attributed string for the textView
- (NSAttributedString*)createAttributedText {

  // Get the localized string with placeholders for links
  NSString *text =
      l10n_util::GetNSString(IDS_VIVALDI_IOS_ONBOARDING_START_AGREEMENT_TITLE);

  // Parse the string to extract the ranges of links
  StringWithTags parsedString = ParseStringWithLinks(text);

  // Create paragraph style with leading alignment
  NSMutableParagraphStyle *paragraphStyle =
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
  paragraphStyle.alignment = NSTextAlignmentCenter;

  // Common text attributes
  NSDictionary *textAttributes = @{
    NSForegroundColorAttributeName : [UIColor secondaryLabelColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
    NSParagraphStyleAttributeName : paragraphStyle,
  };

  // Create the attributed string with the full text
  NSMutableAttributedString *attributedText =
      [[NSMutableAttributedString alloc] initWithString:parsedString.string
                                            attributes:textAttributes];

  // Define the URLs corresponding to the links in the text
  NSArray* urls = @[
    [NSURL URLWithString:vVivaldiTermsOfServiceUrl],
    [NSURL URLWithString:vVivaldiCommunityPrivacyUrl]
  ];

  DCHECK_EQ(parsedString.ranges.size(), [urls count]);
  size_t index = 0;
  for (CrURL* url in urls) {
    [attributedText addAttribute:NSLinkAttributeName
                           value:url
                           range:parsedString.ranges[index]];
    index += 1;
  }

  return [attributedText copy];
}

#pragma mark - UITextViewDelegate

// Handle taps on the UITextView links and call the corresponding actions.
- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
         inRange:(NSRange)characterRange
        interaction:(UITextItemInteraction)interaction {

  NSString *URLString = [URL absoluteString];

  if (URLString == vVivaldiTermsOfServiceUrl) {
    if (self.onTermsDidTap) {
      self.onTermsDidTap(URL,
          l10n_util::GetNSString(
              IDS_VIVALDI_IOS_ONBOARDING_START_AGREEMENT_LICENSE_TITLE));
    }
  } else if (URLString == vVivaldiCommunityPrivacyUrl) {
    if (self.onPrivacyPolicyDidTap) {
      self.onPrivacyPolicyDidTap(URL,
          l10n_util::GetNSString(
                IDS_VIVALDI_IOS_ONBOARDING_START_AGREEMENT_PRIVACY_TITLE));
    }
  }

  // Prevent default action (opening the URL) as we handle it
  // on the callback
  return NO;
}

@end
