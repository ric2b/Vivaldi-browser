// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/vivaldi_create_account_ui_helper.h"

#import "base/json/json_reader.h"
#import "base/json/json_string_value_serializer.h"
#import "ios/ui/settings/sync/vivaldi_sync_settings_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

absl::optional<base::Value> NSDataToDict(NSData* data) {
  const base::StringPiece server_reply(static_cast<const char*>([data bytes]),
                                        [data length]);
  return base::JSONReader::Read(server_reply);
}

void sendRequestToServer(base::Value::Dict dict, NSURL* url,
    ServerRequestCompletionHandler handler, NSURLSessionDataTask* task) {
  std::string json;
  JSONStringValueSerializer serializer(&json);
  serializer.Serialize(base::Value(std::move(dict)));
  NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:url
                          cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
                                      timeoutInterval:vConnectionTimeout];
  NSData* data = [NSData dataWithBytes:json.c_str() length:json.size()];
  [request setHTTPMethod:vHttpMethod];
  [request setHTTPBody:data];
  [request setValue:vRequestValue forHTTPHeaderField:vRequestHeader];

  NSURLSession* session = [NSURLSession sharedSession];
  task =
      [session dataTaskWithRequest:request
                completionHandler:handler];
  [task resume];
}
