// Copyright 2025 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/search_engine/editor/vivaldi_search_engine_editor_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/template_url_service_observer.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/util.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/ui/settings/search_engine/editor/vivaldi_search_engine_editor_swift.h"
#import "prefs/vivaldi_pref_names.h"

namespace {

// Template URL parameter constants
const char kSearchTermsParameterFull[] = "{searchTerms}";
const char kGoogleUnescapedSearchTermsParameterFull[] =
    "{google:unescapedSearchTerms}";

// Display parameter constants for UI
const char kDisplaySearchTerms[] = "%s";
const char kDisplayUnescapedSearchTerms[] = "%S";

/// Converts template URL parameters to display format for UI
std::string ToDisplay(const std::string& turl_param) {
  std::string result(turl_param);
  base::ReplaceSubstringsAfterOffset(&result, 0, kSearchTermsParameterFull,
                                     kDisplaySearchTerms);
  base::ReplaceSubstringsAfterOffset(&result, 0,
                                     kGoogleUnescapedSearchTermsParameterFull,
                                     kDisplayUnescapedSearchTerms);
  return result;
}

/// Converts display format back to template URL parameters
std::string FromDisplay(const std::string& display_string) {
  std::string result(display_string);
  base::ReplaceSubstringsAfterOffset(&result, 0, kDisplaySearchTerms,
                                     kSearchTermsParameterFull);
  base::ReplaceSubstringsAfterOffset(&result, 0, kDisplayUnescapedSearchTerms,
                                     kGoogleUnescapedSearchTermsParameterFull);
  return result;
}

}  // namespace

@interface VivaldiSearchEngineEditorMediator () <SearchEngineObserving> {
  // Profile and service dependencies
  ProfileIOS* _profile;
  TemplateURLService* _templateURLService;

  // Template URL being edited (null for new search engines)
  const TemplateURL* _editingItem;

  // Internal model representation for editing
  VivaldiSearchEngineEditorItem* _editingItemInternal;

  // Observer bridge for template URL service changes
  std::unique_ptr<SearchEngineObserverBridge> _observer;

  // Array of existing search engine nicknames for validation
  NSMutableArray<VivaldiSearchEngineNicknameModel*>* _nicknames;

  // Whether we're editing an existing engine or creating new
  BOOL _isEditing;
}

@end

@implementation VivaldiSearchEngineEditorMediator

#pragma mark - Initialization

- (instancetype)initWithProfile:(ProfileIOS*)profile
                      isEditing:(BOOL)isEditing
                    editingItem:(const TemplateURL*)editingItem {
  self = [super init];
  if (self) {
    _profile = profile;
    _isEditing = isEditing;
    _editingItem = editingItem;

    [self setupTemplateURLService];
  }
  return self;
}

#pragma mark - Public Methods

- (void)saveChanges {
  if (!_editingItemInternal.isFormValid) {
    return;
  }

  [self.consumer searchEngineBackendWillChange];

  if (_isEditing) {
    [self updateExistingSearchEngine];
  } else {
    [self createNewSearchEngine];
  }

  [self.consumer searchEngineBackendDidChange];
}

- (void)disconnect {
  [self cleanup];
}

#pragma mark - Private Setup Methods

- (void)setupTemplateURLService {
  _templateURLService = ios::TemplateURLServiceFactory::GetForProfile(_profile);
  _observer =
      std::make_unique<SearchEngineObserverBridge>(self, _templateURLService);
  _templateURLService->Load();

  if (_templateURLService->loaded()) {
    [self loadSearchEngines];
  }
}

#pragma mark - Private Save Methods

- (void)updateExistingSearchEngine {
  TemplateURL* itemToUpdate =
      _templateURLService->GetTemplateURLForGUID(_editingItem->sync_guid());

  if (!itemToUpdate) {
    return;
  }

  _templateURLService->ResetTemplateURL(
      itemToUpdate, base::SysNSStringToUTF16(_editingItemInternal.name),
      base::SysNSStringToUTF16(_editingItemInternal.nickname),
      FromDisplay(base::SysNSStringToUTF8(_editingItemInternal.url)),
      FromDisplay(base::SysNSStringToUTF8(_editingItemInternal.postParameters)),
      FromDisplay(base::SysNSStringToUTF8(_editingItemInternal.suggestURL)),
      _editingItem->suggestions_url_post_params(),
      FromDisplay(base::SysNSStringToUTF8(_editingItemInternal.imageSearchURL)),
      FromDisplay(base::SysNSStringToUTF8(
          _editingItemInternal.imageSearchPostParameters)),
      TemplateURL::GenerateFaviconURL(
          GURL(base::SysNSStringToUTF8(_editingItemInternal.url))));
}

- (void)createNewSearchEngine {
  TemplateURLData data;
  data.SetShortName(base::SysNSStringToUTF16(_editingItemInternal.name));
  data.SetKeyword(base::SysNSStringToUTF16(_editingItemInternal.nickname));
  data.SetURL(FromDisplay(base::SysNSStringToUTF8(_editingItemInternal.url)));
  data.suggestions_url =
      FromDisplay(base::SysNSStringToUTF8(_editingItemInternal.suggestURL));
  data.image_url =
      FromDisplay(base::SysNSStringToUTF8(_editingItemInternal.imageSearchURL));
  data.search_url_post_params =
      FromDisplay(base::SysNSStringToUTF8(_editingItemInternal.postParameters));
  data.image_url_post_params = FromDisplay(
      base::SysNSStringToUTF8(_editingItemInternal.imageSearchPostParameters));
  data.favicon_url = TemplateURL::GenerateFaviconURL(
      GURL(base::SysNSStringToUTF8(_editingItemInternal.url)));
  data.safe_for_autoreplace = false;

  _templateURLService->Add(std::make_unique<TemplateURL>(data));
}

#pragma mark - Private Helper Methods

- (void)loadSearchEngines {
  std::vector<raw_ptr<TemplateURL, VectorExperimental>> urls =
      _templateURLService->GetTemplateURLs();

  _nicknames = [[NSMutableArray alloc] init];

  for (TemplateURL* url : urls) {
    if (url->is_active() != TemplateURLData::ActiveStatus::kFalse) {
      VivaldiSearchEngineNicknameModel* nicknameObject =
          [self createNicknameModelFromTemplateURL:url];
      [_nicknames addObject:nicknameObject];
    }
  }

  [self.consumer availableNicknamesDidUpdate:[_nicknames copy]];
}

- (VivaldiSearchEngineNicknameModel*)createNicknameModelFromTemplateURL:
    (TemplateURL*)url {
  NSString* guid = base::SysUTF8ToNSString(url->sync_guid());
  NSString* name = base::SysUTF16ToNSString(url->short_name());
  NSString* nickname = base::SysUTF16ToNSString(url->keyword());

  return [[VivaldiSearchEngineNicknameModel alloc] initWithGuid:guid
                                                           name:name
                                                       nickname:nickname];
}

- (VivaldiSearchEngineEditorItem*)createEditorItemFromTemplateURL:
    (const TemplateURL*)templateURL {
  return [[VivaldiSearchEngineEditorItem alloc]
                   initWithGuid:base::SysUTF8ToNSString(
                                    templateURL->sync_guid())
                           name:base::SysUTF16ToNSString(
                                    templateURL->short_name())
                       nickname:base::SysUTF16ToNSString(templateURL->keyword())
                            url:base::SysUTF8ToNSString(
                                    ToDisplay(templateURL->url()))
                     suggestURL:base::SysUTF8ToNSString(
                                    ToDisplay(templateURL->suggestions_url()))
                 postParameters:base::SysUTF8ToNSString(ToDisplay(
                                    templateURL->suggestions_url_post_params()))
                 imageSearchURL:base::SysUTF8ToNSString(
                                    ToDisplay(templateURL->image_url()))
      imageSearchPostParameters:base::SysUTF8ToNSString(ToDisplay(
                                    templateURL->image_url_post_params()))];
}

- (void)cleanup {
  _observer.reset();
  _profile = nullptr;
  _templateURLService = nullptr;
  _editingItem = nil;
  _editingItemInternal = nil;
  _consumer = nil;
  _nicknames = nil;
}

#pragma mark - Property Accessors

- (void)setConsumer:(id<VivaldiSearchEngineEditorConsumer>)consumer {
  _consumer = consumer;

  // Provide initial data to consumer
  [consumer availableNicknamesDidUpdate:[_nicknames copy]];

  // Set up editing item if we're in edit mode
  if (_isEditing && _editingItem) {
    VivaldiSearchEngineEditorItem* localItem =
        [self createEditorItemFromTemplateURL:_editingItem];
    _editingItemInternal = localItem;
    [consumer searchEngineEditorItemDidChange:localItem];
  }
}

#pragma mark - VivaldiSearchEngineEditorConsumer

- (void)searchEngineEditorItemDidChange:(VivaldiSearchEngineEditorItem*)item {
  _editingItemInternal = item;
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  [self loadSearchEngines];
}

@end