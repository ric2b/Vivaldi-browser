// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/modal_page/modal_page_coordinator.h"

#import <WebKit/WebKit.h>

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/ui/modal_page/modal_page_commands.h"
#import "ios/ui/modal_page/modal_page_view_controller.h"
#import "ios/web/common/web_view_creation_util.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ModalPageCoordinator () <UIAdaptivePresentationControllerDelegate,
                              WKNavigationDelegate>

@property(nonatomic, strong) ModalPageViewController* viewController;
@property(nonatomic, strong) NSURL* url;
@property(nonatomic, strong) NSString* title;

@end

@implementation ModalPageCoordinator {
  AlertCoordinator* _alertCoordinator;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                   pageURL:(NSURL*)url
                                     title:(NSString*)title {
  DCHECK(url);
  DCHECK(title);
  self = [super initWithBaseViewController:baseViewController
                                   browser:browser];
  if (self) {
    _url = url;
    _title = title;
  }
  return self;
}

- (void)start {
  id<ModalPageCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ModalPageCommands);
  self.viewController = [[ModalPageViewController alloc]
      initWithContentView:[self newWebViewDisplayingModalPage]
                  handler:handler
                    title:self.title];
  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:self.viewController];
  navigationController.presentationController.delegate = self;

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
}

#pragma mark - Private

// Creates a WKWebView and load the terms of services html page in it.
- (WKWebView*)newWebViewDisplayingModalPage {
  // Create web view.
  WKWebView* webView = web::BuildWKWebView(self.viewController.view.bounds,
                                           self.browser->GetProfile());
  webView.navigationDelegate = self;

  // Loads terms of service into the web view.
  NSURLRequest* request =
      [[NSURLRequest alloc] initWithURL:self.url
                            cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
                        timeoutInterval:60.0];
  [webView loadRequest:request];
  [webView setOpaque:NO];

  return webView;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self closeModalPage];
}

#pragma mark - WKNavigationDelegate

// In case the webpage can’t be loaded
// show an Alert stating "No Internet".
- (void)webView:(WKWebView*)webView
    didFailProvisionalNavigation:(WKNavigation*)navigation
                       withError:(NSError*)error {
  [self failedToLoad];
}

// As long as page don’t include external files (e.g. css or js),
// this method should never be called.
- (void)webView:(WKWebView*)webView
    didFailNavigation:(WKNavigation*)navigation
            withError:(NSError*)error {
  [self failedToLoad];
}

#pragma mark - Private

// If the page can’t be loaded, show an Alert stating "No Internet".
- (void)failedToLoad {
  if (_alertCoordinator) {
    // If the alert is already displayed, don’t display a second one.
    // It should never occur as long as the page don’t include external files.
    return;
  }
  NSString* alertMessage =
      l10n_util::GetNSString(IDS_ERRORPAGES_HEADING_INTERNET_DISCONNECTED);
  _alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:self.viewController
                                                   browser:self.browser
                                                     title:alertMessage
                                                   message:nil];

  __weak __typeof(self) weakSelf = self;
  [_alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_OK)
                               action:^{
                                 [weakSelf stopAlertAndPage];
                               }
                                style:UIAlertActionStyleDefault];

  [_alertCoordinator start];
}

- (void)stopAlertAndPage {
  [_alertCoordinator stop];
  _alertCoordinator = nil;
  [self closeModalPage];
}

- (void)closeModalPage {
  id<ModalPageCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ModalPageCommands);
  [handler closeModalPage];
}

@end
