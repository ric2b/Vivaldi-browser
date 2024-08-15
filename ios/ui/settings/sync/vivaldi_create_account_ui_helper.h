// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SYNC_VIVALDI_CREATE_ACCOUNT_UI_HELPER_H_
#define IOS_UI_SETTINGS_SYNC_VIVALDI_CREATE_ACCOUNT_UI_HELPER_H_

#import <Foundation/Foundation.h>

#import "base/values.h"

typedef void (^ServerRequestCompletionHandler)
    (NSData* data, NSURLResponse* response, NSError* error);

std::optional<base::Value> NSDataToDict(NSData* data);

void sendRequestToServer(base::Value::Dict dict, NSURL* url,
    ServerRequestCompletionHandler handler, NSURLSessionDataTask* task);

NSString* GetDocumentsFolderPath();

#endif // IOS_UI_SETTINGS_SYNC_VIVALDI_CREATE_ACCOUNT_UI_HELPER_H_