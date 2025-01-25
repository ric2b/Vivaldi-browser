// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/copy_to_note/copy_to_note_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/notes/note_node.h"
#import "components/notes/notes_model.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/browser_container/edit_menu_alert_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_view_ios.h"
#import "ios/notes/notes_factory.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

namespace {
typedef void (^ProceduralBlockWithItemArray)(NSArray<UIMenuElement*>*);
typedef void (^ProceduralBlockWithBlockWithItemArray)(
    ProceduralBlockWithItemArray);
}  // namespace

@interface CopyToNoteMediator()
@property(nonatomic, assign) Browser* browser;
@property(nonatomic, assign) vivaldi::NotesModel* notesModel;
@end

@implementation CopyToNoteMediator

- (instancetype)initWithBrowser:(Browser*)browser {
  self = [super init];
  if (self) {
    DCHECK(browser);
    self.browser = browser;
    ProfileIOS* profile = browser->GetProfile();
    self.notesModel = vivaldi::NotesModelFactory::GetForProfile(profile);
  }
  return self;
}

- (void)dealloc {
  self.browser = nil;
  self.notesModel = nil;
}

- (void)handleCopyToNoteSelection {
  UIResponder* firstResponder = GetFirstResponder();
  if ([firstResponder isKindOfClass:[OmniboxTextFieldIOS class]]) {
    OmniboxTextFieldIOS* field =
        base::apple::ObjCCast<OmniboxTextFieldIOS>(firstResponder);
    NSString* text = [field textInRange:field.selectedTextRange];
    if (self.notesModel) {
      const vivaldi::NoteNode* defaultNoteFolder = self.notesModel->main_node();
      if (defaultNoteFolder) {
        self.notesModel->AddNote(defaultNoteFolder,
                                 defaultNoteFolder->children().size(),
                                 base::SysNSStringToUTF16(text),
                                 GURL(), base::SysNSStringToUTF16(text));
      }
    }
  } else {
    __weak __typeof(self) weakSelf = self;
    void (^javascript_completion)(const base::Value*) =
      ^(const base::Value* value) {
        if (weakSelf.notesModel) {
          const vivaldi::NoteNode* defaultNoteFolder =
              weakSelf.notesModel->main_node();
          if (defaultNoteFolder) {
            weakSelf.notesModel->AddNote(defaultNoteFolder,
                                         defaultNoteFolder->children().size(),
                                         base::UTF8ToUTF16(value->GetString()),
                                         GURL(),
                                         base::UTF8ToUTF16(value->GetString()));
          }
        }
      };

    web::WebState* currentWebState =
        self.browser->GetWebStateList()->GetActiveWebState();
    web::WebFrame* main_frame =
        currentWebState->GetPageWorldWebFramesManager()->GetMainWebFrame();
    if (main_frame) {
      main_frame->ExecuteJavaScript(
          base::SysNSStringToUTF16(@"window.getSelection().toString()"),
          base::BindOnce(javascript_completion));
    }
  }
}

- (void)addItemWithCompletion:(ProceduralBlockWithItemArray)completion {
  __weak __typeof(self) weakSelf = self;
  NSString* title = l10n_util::GetNSString(IDS_VIVALDI_COPY_TO_NOTE);
  NSString* copyToNoteId = @"chromecommand.copytonote";
  UIAction* action =
      [UIAction actionWithTitle:title
                          image:nil
                     identifier:copyToNoteId
                        handler:^(UIAction* a) {
                          [weakSelf handleCopyToNoteSelection];
                        }];
  completion(@[ action ]);
}

#pragma mark - EditMenuProvider

- (void)buildMenuWithBuilder:(id<UIMenuBuilder>)builder {
  NSString* copyToNoteId = @"chromecommand.menu.copytonote";

  __weak __typeof(self) weakSelf = self;
  ProceduralBlockWithBlockWithItemArray provider =
      ^(ProceduralBlockWithItemArray completion) {
        [weakSelf addItemWithCompletion:completion];
      };
  // Use a deferred element so that the item is displayed depending on the text
  // selection and updated on selection change.
  UIDeferredMenuElement* deferredMenuElement =
      [UIDeferredMenuElement elementWithProvider:provider];

  UIMenu* copyToNoteMenu = [UIMenu menuWithTitle:@""
                                           image:nil
                                      identifier:copyToNoteId
                                         options:UIMenuOptionsDisplayInline
                                        children:@[ deferredMenuElement ]];
  [builder insertChildMenu:copyToNoteMenu atEndOfMenuForIdentifier:UIMenuRoot];
}

@end
