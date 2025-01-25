// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/feedback_prompt/vivaldi_feedback_view_mediator.h"

#import "Foundation/Foundation.h"

#import "ios/ui/feedback_prompt/vivaldi_feedback_view_consumer.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"

namespace {

// HTTP Client call constants
NSString* const kFeedbackURL = @"https://feedback.vivaldi.com";
NSString * const HTTPMethod = @"POST";
NSString * const contentType = @"application/json; charset=utf-8";
NSString * const contentTypeKey = @"Content-Type";
const NSTimeInterval requestTimeout = 60.0;
const NSTimeInterval resourceTimeout = 60.0;
const NSInteger successCode = 200;

// Request data constants
static NSString *const kPropDeviceType = @"deviceType";
static NSString *const kPropDeviceTheme = @"deviceTheme";
static NSString *const kPropDeviceOrientation = @"deviceOrientation";
static NSString *const kPropOSVersion = @"osVersion";
static NSString *const kPropDevice = @"device";
static NSString *const kPropBrand = @"brand";
static NSString *const kPropAppVersion = @"appVersion";
static NSString *const kPropIssueType = @"issueType";
static NSString *const kPropIssueSubType = @"issueSubType";
static NSString *const kPropFeedback = @"feedback";
}

@implementation VivaldiFeedbackViewMediator

- (instancetype)init {
  return [super init];
}

- (void)handleSubmitTapWithParentIssue:(VivaldiFeedbackIssue*)parentIssue
                            childIssue:(VivaldiFeedbackIssue*)childIssue
                               message:(NSString*)message {
  // Prepare JSON data
  NSMutableDictionary *jsonDict = [NSMutableDictionary dictionary];
  jsonDict[kPropDeviceType] = [self getDeviceType];
  jsonDict[kPropDeviceTheme] = [self getDeviceColorScheme];
  jsonDict[kPropDeviceOrientation] = [self getDeviceOrientation];
  jsonDict[kPropOSVersion] = [self getOSVersion];
  jsonDict[kPropBrand] = [self getBrand];
  jsonDict[kPropDevice] = [self getDeviceName];
  jsonDict[kPropAppVersion] = [self getAppVersion];
  jsonDict[kPropIssueType] = [parentIssue titleStringForSubmission];
  NSString *issueSubType = [childIssue titleStringForSubmission];
  if (issueSubType) {
    jsonDict[kPropIssueSubType] = issueSubType;
  }
  jsonDict[kPropFeedback] = message;

  NSError *error;
  NSData *jsonData =
      [NSJSONSerialization dataWithJSONObject:jsonDict
                                      options:0
                                        error:&error];
  if (error) {
    [self onFeedbackSubmitted];
    return;
  }

  // Send feedback to backend
  NSURL *url = [NSURL URLWithString:kFeedbackURL];
  __weak __typeof(self) weakSelf = self;
  [self httpClientRequestWithURL:url
                        jsonData:jsonData
                      completion:^(NSString *response) {
    [weakSelf onFeedbackSubmitted];
  }];
}

- (void)disconnect {
  // No cleanup needed.
}


#pragma mark - Properties
- (void)setConsumer:(id<VivaldiFeedbackViewConsumer>)consumer {
  _consumer = consumer;
}

#pragma mark - Private Helpers

- (void)httpClientRequestWithURL:(NSURL*)url
                        jsonData:(NSData*)jsonData
                      completion:(void (^)(NSString *response))completion {

  NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];
  request.HTTPMethod = HTTPMethod;
  [request setValue:contentType forHTTPHeaderField:contentTypeKey];
  request.HTTPBody = jsonData;
  request.timeoutInterval = requestTimeout;

  NSURLSessionConfiguration *configuration =
      [NSURLSessionConfiguration defaultSessionConfiguration];
  configuration.timeoutIntervalForRequest = requestTimeout;
  configuration.timeoutIntervalForResource = resourceTimeout;

  NSURLSession *session = [NSURLSession sessionWithConfiguration:configuration];

  NSURLSessionDataTask *task =
      [session dataTaskWithRequest:request
                 completionHandler:^(NSData *data,
                                     NSURLResponse *responseObj,
                                     NSError *error) {
    NSString *responseString = @"";
    if (!error) {
      NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse*)responseObj;
      if (httpResponse.statusCode == successCode && data) {
        responseString =
            [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
      }
    }

    dispatch_async(dispatch_get_main_queue(), ^{
      if (completion) {
        completion(responseString);
      }
    });
  }];

  [task resume];
}

- (void)onFeedbackSubmitted {
  [self.consumer issueDidSubmitSuccessfully:YES];
}

// MARK: Device and App information methods

- (NSString*)getDeviceType {
  UIUserInterfaceIdiom idiom = [[UIDevice currentDevice] userInterfaceIdiom];
  switch (idiom) {
    case UIUserInterfaceIdiomPhone:
      return @"iPhone";
    case UIUserInterfaceIdiomPad:
      return @"iPad";
    case UIUserInterfaceIdiomTV:
      return @"Apple TV";
    case UIUserInterfaceIdiomCarPlay:
      return @"CarPlay";
    case UIUserInterfaceIdiomMac:
      return @"Mac";
    default:
      return @"Unspecified";
  }
}

- (NSString*)getDeviceOrientation {
  UIDeviceOrientation orientation = [[UIDevice currentDevice] orientation];
  switch (orientation) {
    case UIDeviceOrientationPortrait:
      return @"Portrait";
    case UIDeviceOrientationPortraitUpsideDown:
      return @"Portrait Upside Down";
    case UIDeviceOrientationLandscapeLeft:
      return @"Landscape Left";
    case UIDeviceOrientationLandscapeRight:
      return @"Landscape Right";
    case UIDeviceOrientationFaceUp:
      return @"Face Up";
    case UIDeviceOrientationFaceDown:
      return @"Face Down";
    case UIDeviceOrientationUnknown:
    default:
      return @"Unknown";
  }
}

- (NSString*)getDeviceColorScheme {
  UIUserInterfaceStyle interfaceStyle =
      [VivaldiGlobalHelpers keyWindow].traitCollection.userInterfaceStyle;
  switch (interfaceStyle) {
    case UIUserInterfaceStyleLight:
      return @"Light";
    case UIUserInterfaceStyleDark:
      return @"Dark";
    case UIUserInterfaceStyleUnspecified:
    default:
      return @"Unspecified";
  }
}

- (NSString*)getOSVersion {
  return [[UIDevice currentDevice] systemVersion];
}

- (NSString*)getBrand {
  return @"Apple";
}

- (NSString*)getDeviceName {
  return [[UIDevice currentDevice] name];
}

- (NSString*)getAppVersion {
  NSDictionary *infoDictionary = [[NSBundle mainBundle] infoDictionary];
  NSString *appVersion = infoDictionary[@"CFBundleShortVersionString"];
  NSString *buildNumber = infoDictionary[@"CFBundleVersion"];
  return [NSString stringWithFormat:@"%@ (%@)", appVersion, buildNumber];
}

@end
