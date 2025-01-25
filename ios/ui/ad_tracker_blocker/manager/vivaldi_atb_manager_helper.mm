// Copyright 2023 Vivaldi Technologies AS. All rights reserved.

#import "ios/ui/ad_tracker_blocker/manager/vivaldi_atb_manager_helper.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using l10n_util::GetNSString;
using l10n_util::GetNSStringF;

@interface ATBSourceTitleAndOrigin ()

@property (nonatomic, copy) NSString *title;
@property (nonatomic, assign) NSInteger stringId;

@end

@implementation ATBSourceTitleAndOrigin

- (instancetype)initWithTitle:(NSString*)title
                     stringId:(NSInteger)stringId {
    self = [super init];
    if (self) {
        _title = [title copy];
        _stringId = stringId;
    }
    return self;
}

@end

@interface VivaldiATBManagerHelper()
@property (nonatomic,copy,readonly) NSString *defaultDomain;
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
    return [[ATBSourceTitleAndOrigin alloc] initWithTitle:title
                                                 stringId:stringId];
}

+ (NSString*)defaultDomain {
  VivaldiATBManagerHelper *instance = [[VivaldiATBManagerHelper alloc] init];
  return instance.defaultDomain;
}

+ (NSDictionary<NSString*, ATBSourceTitleAndOrigin*>*)sourcesMap {
  VivaldiATBManagerHelper *instance = [[VivaldiATBManagerHelper alloc] init];
  return instance.sourcesMap;
}

+ (NSDictionary<NSString*, ATBSourceTitleAndOrigin*>*)createSourcesMap {
  NSMutableDictionary *map = [NSMutableDictionary new];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"DuckDuckGo Tracker Radar"
                    stringId:-1]
          forKey:[@"https://downloads.vivaldi.com/"
                  stringByAppendingString:@"ddg/tds-v2-current.json"]];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"EasyPrivacy"
                    stringId:-1]
          forKey:[@"https://downloads.vivaldi.com/"
                  stringByAppendingString:@"easylist/easyprivacy-current.txt"]];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"EasyList"
                    stringId:-1]
          forKey:[@"https://downloads.vivaldi.com/"
                  stringByAppendingString:@"easylist/easylist-current.txt"]];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"ABP anti-circumvention list"
                    stringId:-1]
          forKey:[@"https://downloads.vivaldi.com/"
                    stringByAppendingString:
                    @"lists/abp/abp-filters-anti-cv-current.txt"]];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"AdBlock Warning Removal List"
                    stringId:-1]
          forKey:[@"https://downloads.vivaldi.com/"
                    stringByAppendingString:
                    @"lists/abp/antiadblockfilters-current.txt"]];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@""
                    stringId:
                    IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_ALLOW_VIVALDI_PARTNERS]
          forKey:[@"https://downloads.vivaldi.com/"
                    stringByAppendingString:
                    @"lists/vivaldi/partners-current.txt"]];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"I don’t care about cookies"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_TYPE_COOKIE_WARNING]
          forKey:@"https://www.i-dont-care-about-cookies.eu/abp/"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"Easylist Cookie List"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_TYPE_COOKIE_WARNING]
          forKey:@"https://secure.fanboy.co.nz/fanboy-cookiemonster.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"Fanboy's Annoyance List"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_TYPE_ANNOYANCES]
          forKey:@"https://secure.fanboy.co.nz/fanboy-annoyance.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"Liste AR"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_ARABIC]
          forKey:@"https://easylist-downloads.adblockplus.org/Liste_AR.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"Bulgarian List"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_BULGARIAN]
          forKey:@"http://stanev.org/abp/adblock_bg.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"EasyList China"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_CHINESE]
          forKey:
   @"https://easylist-downloads.adblockplus.org/easylistchina.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"CJX’s Annoyance List"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_CHINESE]
          forKey:
   @"https://raw.githubusercontent.com/cjx82630/cjxlist/master/cjx-annoyance.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"Colombian List by Yecarrillo"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_COLOMBIAN]
          forKey:
   @"https://raw.githubusercontent.com/yecarrillo/adblock-colombia/master/adblock_co.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"Czech and Slovak filter list"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_CZECH_AND_SLOVAK]
          forKey:
   @"https://raw.githubusercontent.com/tomasko126/easylistczechandslovak/master/filters.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"EasyList Dutch"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_DUTCH]
          forKey:
   @"https://easylist-downloads.adblockplus.org/easylistdutch.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"Peter Lowe’s List"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_ENGLISH]
          forKey:
   @"https://pgl.yoyo.org/adservers/serverlist.php?hostformat=adblockplus&mimetype=plaintext"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"Eesti saitidele kohandatud filter"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_ESTONIAN]
          forKey:@"https://adblock.ee/list.php"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"Filters by Gurud.ee"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_ESTONIAN]
          forKey:@"https://gurud.ee/ab.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"AdBlock Farsi"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_FARSI]
          forKey:
   @"https://cdn.rawgit.com/SlashArash/adblockfa/master/adblockfa.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"EasyList Finland"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_FINNISH]
          forKey:
   @"https://raw.githubusercontent.com/finnish-easylist-addition/finnish-easylist-addition/master/Finland_adb.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"Liste FR"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_FRENCH]
          forKey:@"https://easylist-downloads.adblockplus.org/liste_fr.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"EasyList Germany"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_GERMAN]
          forKey:@"https://easylist.to/easylistgermany/easylistgermany.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"void.gr"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_GREEK]
          forKey:@"https://www.void.gr/kargig/void-gr-filters.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"EasyList Hebrew"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_HEBREW]
          forKey:
   @"https://raw.githubusercontent.com/easylist/EasyListHebrew/master/EasyListHebrew.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"hufilter"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_HUNGARIAN]
          forKey:
   @"https://raw.githubusercontent.com/hufilter/hufilter/refs/heads/gh-pages/hufilter.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"Icelandic ABP List"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_ICELANDIC]
          forKey:@"https://adblock.gardar.net/is.abp.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"Indian List"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_INDIAN]
          forKey:@"https://easylist-downloads.adblockplus.org/indianlist.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"ABPindo"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_INDONESIAN]
          forKey:
   @"https://raw.githubusercontent.com/heradhis/indonesianadblockrules/master/subscriptions/abpindo.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"X Files"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_ITALIAN]
          forKey:
   @"https://raw.githubusercontent.com/gioxx/xfiles/master/filtri.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"Indian List"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_ITALIAN]
          forKey:
   @"https://easylist-downloads.adblockplus.org/easylistitaly.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"ABP Japanese filters"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_JAPANESE]
          forKey:
   @"https://raw.githubusercontent.com/k2jp/abp-japanese-filters/master/abpjf.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"もちフィルタ（広告ブロック）"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_JAPANESE]
          forKey:
   @"https://raw.githubusercontent.com/eEIi0A5L/adblock_filter/master/mochi_filter.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"たまごフィルタ（mobile filter）"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_JAPANESE]
          forKey:
   @"https://raw.githubusercontent.com/eEIi0A5L/adblock_filter/master/tamago_filter.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"Korean List"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_KOREAN]
          forKey:@"https://easylist-downloads.adblockplus.org/koreanlist.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"YousList"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_KOREAN]
          forKey:
   @"https://raw.githubusercontent.com/yous/YousList/master/youslist.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"Latvian List"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_LATVIAN]
          forKey:
   @"https://notabug.org/latvian-list/adblock-latvian/raw/master/lists/latvian-list.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"EasyList Lithuania"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_LITHUANIAN]
          forKey:
   @"https://raw.githubusercontent.com/EasyList-Lithuania/easylist_lithuania/master/easylistlithuania.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"Dandelion Sprout’s Nordic Filters"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_NORDIC]
          forKey:
   @"https://raw.githubusercontent.com/DandelionSprout/adfilt/master/NorwegianExperimentalList%20alternate%20versions/NordicFiltersABP.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"EasyList Polish"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_POLISH]
          forKey:
   @"https://easylist-downloads.adblockplus.org/easylistpolish.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"EasyList Portuguese"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_PORTUGUESE]
          forKey:
   @"https://easylist-downloads.adblockplus.org/easylistportuguese.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"ROLIST"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_ROMANIAN]
          forKey:@"https://www.zoso.ro/pages/rolist.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"RU AdList"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_RUSSIAN]
          forKey:@"https://easylist-downloads.adblockplus.org/advblock.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"EasyList Spanish"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_SPANISH]
          forKey:
   @"https://easylist-downloads.adblockplus.org/easylistspanish.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"Fanboy’s Turkish"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_TURKISH]
          forKey:@"https://secure.fanboy.co.nz/fanboy-turkish.txt"];

  [map setObject:[VivaldiATBManagerHelper
                  createEntryWithTitle:@"ABPVN List"
                    stringId:IDS_VIVALDI_IOS_SOURCE_MAP_ORIGIN_VIETNAMESE]
          forKey:
   @"https://raw.githubusercontent.com/abpvn/abpvn/master/filter/abpvn.txt"];

  return [map copy];
}

- (NSString*)titleForSourceForKey:(NSString*)key {
  ATBSourceTitleAndOrigin *titleAndOrigin = self.sourcesMap[key];

  if (titleAndOrigin != nil) {
    if (titleAndOrigin.stringId < 0 ) {
      NSString* description = titleAndOrigin.title;
      return description;
    } else if (titleAndOrigin.title.length != 0) {
      NSString* description = GetNSStringF(
            titleAndOrigin.stringId,
            base::SysNSStringToUTF16(titleAndOrigin.title));
      return description;
    } else {
      NSString* description = GetNSString(titleAndOrigin.stringId);
      return description;
    }
  } else {
    NSString* description = key;
    return description;
  }
}

@end
