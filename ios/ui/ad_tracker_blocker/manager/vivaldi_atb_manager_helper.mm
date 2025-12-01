// Copyright 2023 Vivaldi Technologies AS. All rights reserved.

#import "ios/ui/ad_tracker_blocker/manager/vivaldi_atb_manager_helper.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using l10n_util::GetNSString;
using l10n_util::GetNSStringF;

@interface ATBSourceTitleAndOrigin ()

@property(nonatomic, copy) NSString* title;
@property(nonatomic, assign) NSInteger stringId;

@end

@implementation ATBSourceTitleAndOrigin

- (instancetype)initWithTitle:(NSString*)title stringId:(NSInteger)stringId {
  self = [super init];
  if (self) {
    _title = [title copy];
    _stringId = stringId;
  }
  return self;
}

@end

@interface VivaldiATBManagerHelper ()
@property(nonatomic, copy, readonly) NSString* defaultDomain;
@end

@implementation VivaldiATBManagerHelper
@synthesize defaultDomain = _defaultDomain;

- (instancetype)init {
  self = [super init];
  if (self) {
    _sourcesMap = [VivaldiATBManagerHelper createSourcesMap];
    _defaultDomain = @"https://downloads.vivaldi.com/";
  }
  return self;
}

+ (ATBSourceTitleAndOrigin*)createEntryWithTitle:(NSString*)title
                                        stringId:(NSInteger)stringId {
  return
      [[ATBSourceTitleAndOrigin alloc] initWithTitle:title stringId:stringId];
}

+ (NSString*)defaultDomain {
  VivaldiATBManagerHelper* instance = [[VivaldiATBManagerHelper alloc] init];
  return instance.defaultDomain;
}

+ (NSDictionary<NSString*, ATBSourceTitleAndOrigin*>*)sourcesMap {
  VivaldiATBManagerHelper* instance = [[VivaldiATBManagerHelper alloc] init];
  return instance.sourcesMap;
}

+ (NSDictionary<NSString*, ATBSourceTitleAndOrigin*>*)createSourcesMap {
  NSMutableDictionary* map = [NSMutableDictionary new];

  [map setObject:[VivaldiATBManagerHelper
                     createEntryWithTitle:@"DuckDuckGo Tracker Radar"
                                 stringId:-1]
          forKey:@"bb9915f1-2e78-4fa2-9c84-b5022e7b0b09"];

  [map setObject:[VivaldiATBManagerHelper createEntryWithTitle:@"EasyPrivacy"
                                                      stringId:-1]
          forKey:@"18d85f61-147a-4aa6-aa6d-607f5ef79eef"];

  [map setObject:[VivaldiATBManagerHelper createEntryWithTitle:@"EasyList"
                                                      stringId:-1]
          forKey:@"8bef4dce-11b7-4452-8055-80eae7939dfb"];

  [map setObject:[VivaldiATBManagerHelper
                     createEntryWithTitle:@"ABP anti-circumvention list"
                                 stringId:-1]
          forKey:@"3b9a77ec-f9cc-4626-ace3-bd2e2d679d4c"];

  [map setObject:[VivaldiATBManagerHelper
                     createEntryWithTitle:@"AdBlock Warning Removal List"
                                 stringId:-1]
          forKey:@"bbf1b5e9-5adf-4b83-abaf-88adc681dcdf"];

  [map
      setObject:
          [VivaldiATBManagerHelper
              createEntryWithTitle:@""
                          stringId:
                              IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_ALLOW_VIVALDI_PARTNERS]
         forKey:@"9c67efc8-09e5-4e70-b525-fb8c147fb2c2"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"Easylist Cookie List"
                           stringId:
                               IDS_VIVALDI_IOS_SOURCE_MAP_TYPE_COOKIE_WARNING]
          forKey:@"78610306-e2ab-4147-9a10-fb6072e6675e"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"Fanboy's Annoyance List"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_TYPE_ANNOYANCES]
          forKey:@"269f589f-0a17-4158-a961-ee5252120dad"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"Liste AR"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_ARABIC]
          forKey:@"01b357a7-eddb-4dce-9c3f-4e90099bbfcd"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"Bulgarian List"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_BULGARIAN]
          forKey:@"e5d554e9-8249-47c1-abf8-004cd29f4172"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"EasyList China"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_CHINESE]
          forKey:@"439f5af1-9c74-4606-9b9e-b46863ac611c"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"CJX’s Annoyance List"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_CHINESE]
          forKey:@"923b5982-519e-4c7f-9854-3bd354b368b8"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"Colombian List by Yecarrillo"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_COLOMBIAN]
          forKey:@"d0b816af-f803-4efa-9b8b-39bd1a0d5c75"];

  [map
      setObject:
          [VivaldiATBManagerHelper
              createEntryWithTitle:@"Czech and Slovak filter list"
                          stringId:
                              IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_CZECH_AND_SLOVAK]
         forKey:@"5c9b517d-5182-401a-aee6-ae32414ca708"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"EasyList Dutch"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_DUTCH]
          forKey:@"acf379b6-2c46-4802-88c9-6dd46bedfb32"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"Peter Lowe’s List"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_ENGLISH]
          forKey:@"9c486dda-1589-4886-a40c-1be6484eb43d"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"Eesti saitidele kohandatud filter"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_ESTONIAN]
          forKey:@"288bb849-ca3b-4a6c-8c26-8f0f41e88af7"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"Filters by Gurud.ee"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_ESTONIAN]
          forKey:@"d8d2b8a5-f918-4a5f-b03c-0ee921aec48f"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"AdBlock Farsi"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_FARSI]
          forKey:@"0979cdbb-6581-4f56-a57b-f7dc16fb47f8"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"EasyList Finland"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_FINNISH]
          forKey:@"c43fb9ca-bf75-4f07-ad52-1c79cd67a454"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"Liste FR"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_FRENCH]
          forKey:@"9be6251e-631e-4177-abec-d5dbef6be4f7"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"EasyList Germany"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_GERMAN]
          forKey:@"933d897d-cb29-4282-a4f9-2451d83d1885"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"void.gr"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_GREEK]
          forKey:@"9cc5cd12-945e-4948-8ae4-266a21c9165c"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"EasyList Hebrew"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_HEBREW]
          forKey:@"22263ec8-d105-418a-a187-36f5c9808dcf"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"hufilter"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_HUNGARIAN]
          forKey:@"5ec4c886-a4b7-4fd4-9654-a7a138bf74bf"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"Icelandic ABP List"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_ICELANDIC]
          forKey:@"9bd24163-31fe-4889-b7e3-99e5bf742150"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"Indian List"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_INDIAN]
          forKey:@"98ed727f-d9c0-4bc6-bded-19b14b52d167"];

  [map
      setObject:
          [VivaldiATBManagerHelper
              createEntryWithTitle:@"ABPindo"
                          stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_INDONESIAN]
         forKey:@"f7bc721e-5cd1-440c-8036-50813c063929"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"X Files"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_ITALIAN]
          forKey:@"53e46eb7-be5f-41b7-994c-d3155fc2025e"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"Indian List"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_ITALIAN]
          forKey:@"364fff45-270d-4a62-a449-982856057678"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"ABP Japanese filters"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_JAPANESE]
          forKey:@"2450843a-66fb-4e8c-9c65-bdc530623690"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"もちフィルタ（広告ブロック）"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_JAPANESE]
          forKey:@"88f940b8-990c-4caa-abff-bcdb0bfd9276"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"たまごフィルタ（mobile filter）"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_JAPANESE]
          forKey:@"366ed9e8-aa6e-4fd2-b3ff-bdc151f48fa9"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"Korean List"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_KOREAN]
          forKey:@"629f497d-0660-4b7d-8c82-afaf89345681"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"YousList"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_KOREAN]
          forKey:@"aa16a0f8-9ecf-40c1-9062-d72c153145af"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"Latvian List"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_LATVIAN]
          forKey:@"1810bcfd-dad7-4c42-82bb-0fc33ebe7892"];

  [map
      setObject:
          [VivaldiATBManagerHelper
              createEntryWithTitle:@"EasyList Lithuania"
                          stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_LITHUANIAN]
         forKey:@"4f1dbb65-d152-46c8-81db-b5f2cd6d66d5"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"Dandelion Sprout’s Nordic Filters"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_NORDIC]
          forKey:@"a93efa90-ebea-4df2-a1a4-972445bc6d0f"];

  [map setObject:
           [VivaldiATBManagerHelper
               createEntryWithTitle:@"EasyList Polish"
                           stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_POLISH]
          forKey:@"ef6d3c42-e166-4901-9b03-58f124fbebf3"];

  [map
      setObject:
          [VivaldiATBManagerHelper
              createEntryWithTitle:@"EasyList Portuguese"
                          stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_PORTUGUESE]
         forKey:@"3eae7230-473c-4ccd-a15f-f08e4bb86f71"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"ROLIST"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_ROMANIAN]
          forKey:@"434d57a1-51ac-480f-a5af-cc1c127f0313"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"RU AdList"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_RUSSIAN]
          forKey:@"a3d2a41d-6659-4465-9819-ba8317185118"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"EasyList Spanish"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_SPANISH]
          forKey:@"3eae7230-473c-4ccd-a15f-f08e4bb86f71"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"Fanboy’s Turkish"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_TURKISH]
          forKey:@"c29c4544-679b-4335-94f2-b27c7d099803"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"ABPVN List"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_VIETNAMESE]
          forKey:@"092a3a7f-b452-47e2-bbd7-b61e902ad0fd"];

  return [map copy];
}

- (NSString*)titleForSourceForKey:(NSString*)key
                        sourceURL:(NSString*)sourceURL
                      unsafeTitle:(NSString*)unsafeTitle {
  ATBSourceTitleAndOrigin* titleAndOrigin = self.sourcesMap[key];

  if (titleAndOrigin != nil) {
    if (titleAndOrigin.stringId < 0) {
      NSString* description = titleAndOrigin.title;
      return description;
    } else if (titleAndOrigin.title.length != 0) {
      NSString* description =
          GetNSStringF(titleAndOrigin.stringId,
                       base::SysNSStringToUTF16(titleAndOrigin.title));
      return description;
    } else {
      NSString* description = GetNSString(titleAndOrigin.stringId);
      return description;
    }
  } else if ([unsafeTitle length] == 0) {
    return unsafeTitle;
  } else {
    return sourceURL;
  }
}

@end
