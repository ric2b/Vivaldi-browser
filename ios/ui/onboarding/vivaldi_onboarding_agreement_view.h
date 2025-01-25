// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_ONBOARDING_VIVALDI_ONBOARDING_AGREEMENT_VIEW_H_
#define IOS_UI_ONBOARDING_VIVALDI_ONBOARDING_AGREEMENT_VIEW_H_

#import <UIKit/UIKit.h>

/// A custom UIView class that displays an agreement text
/// with tappable links for the terms and privacy policy.
@interface VivaldiOnboardingAgreementView: UIView <UITextViewDelegate>

// Textview to show the attributed text
@property (nonatomic, strong) UITextView* _Nonnull textView;

// Callbacks for handling taps on terms and privacy policy links.
@property (nonatomic, copy, nullable)
    void (^onTermsDidTap)(NSURL* _Nonnull url, NSString* _Nonnull title);
@property (nonatomic, copy, nullable)
    void (^onPrivacyPolicyDidTap)(NSURL* _Nonnull url,
                                  NSString* _Nonnull title);

@end

#endif  // IOS_UI_ONBOARDING_VIVALDI_ONBOARDING_AGREEMENT_VIEW_H_
