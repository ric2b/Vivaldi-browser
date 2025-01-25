// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/notes/markdown/vivaldi_notes_web_view_creation_util.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/ui/notes/markdown/markdown_webview_input_view_protocols.h"
#import "ios/ui/notes/markdown/vivaldi_notes_web_view.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/public/web_client.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

namespace web {

namespace {

// Verifies the preconditions for creating a WKWebView. Must be called before
// a WKWebView is allocated. Not verifying the preconditions before creating
// a WKWebView will lead to undefined behavior.
void VerifyWKWebViewCreationPreConditions(
    BrowserState* browser_state,
    WKWebViewConfiguration* configuration) {
  DCHECK(browser_state);
  DCHECK(configuration);
  WKWebViewConfigurationProvider& config_provider =
      WKWebViewConfigurationProvider::FromBrowserState(browser_state);
  DCHECK_EQ([config_provider.GetWebViewConfiguration() processPool],
            [configuration processPool]);
}

}  // namespace

WKWebView* BuildNotesWKWebView(
    CGRect frame,
    BrowserState* browser_state,
    id<MarkdownInputViewProvider> input_view_provider) {
  DCHECK(browser_state);

  WKWebViewConfigurationProvider& config_provider =
      WKWebViewConfigurationProvider::FromBrowserState(browser_state);
  WKWebViewConfiguration* configuration =
      config_provider.GetWebViewConfiguration();

  VerifyWKWebViewCreationPreConditions(browser_state, configuration);

  GetWebClient()->PreWebViewCreation();

  VivaldiNotesWebView* web_view =
      [[VivaldiNotesWebView alloc] initWithFrame:frame
                                   configuration:configuration];
  web_view.inputViewProvider = input_view_provider;

  // Set the user agent type.
  web_view.customUserAgent = base::SysUTF8ToNSString(
      web::GetWebClient()->GetUserAgent(UserAgentType::MOBILE));

  return web_view;
}

}  // namespace web
