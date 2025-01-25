// Copyright 2022 Vivaldi Technologies. All rights reserved.


#import "ios/chrome/browser/ui/browser_view/browser_view_controller+vivaldi.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import <memory>
#import <objc/runtime.h>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/notes/note_node.h"
#import "components/notes/notes_model.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_view_ios.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/notes/notes_factory.h"
#import "ios/ui/notes/note_interaction_controller.h"
#import "ios/web/common/uikit_ui_util.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

namespace {
const void* const key = &key;

}

@implementation BrowserViewController(Vivaldi)

NoteInteractionController* _noteInteractionController;

// Initializes the note interaction controller if not already initialized.
- (void)showNotesManager:(Browser*)browser
        parentController:(BrowserViewController*)bvc{
    if (_noteInteractionController) {
        [_noteInteractionController showNotes];
        return;
   }
   _noteInteractionController =
      [[NoteInteractionController alloc] initWithBrowser:browser
                                            parentController:bvc];
   [_noteInteractionController showNotes];
}

#if !defined(__IPHONE_16_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_16_0
- (void)onCopyToNote:(UIMenuController*)sender {
  UIResponder* firstResponder = GetFirstResponder();
  vivaldi::NotesModel* notesModel =
    vivaldi::NotesModelFactory::GetForBrowserState(
                              self.browserState);

  if ([firstResponder isKindOfClass:[OmniboxTextFieldIOS class]]) {
    OmniboxTextFieldIOS* field =
      base::apple::ObjCCast<OmniboxTextFieldIOS>(firstResponder);
    NSString* text = [field textInRange:field.selectedTextRange];
    if (notesModel) {
      const vivaldi::NoteNode* defaultNoteFolder =
      notesModel->main_node();
      if (defaultNoteFolder) {
        notesModel->AddNote(defaultNoteFolder,
                     defaultNoteFolder->children().size(),
                     base::SysNSStringToUTF16(text),
                     GURL(), base::SysNSStringToUTF16(text));
      }
    }
  } else {
    void (^javascript_completion)(const base::Value*) =
    ^(const base::Value* value) {
      if (notesModel) {
        const vivaldi::NoteNode* defaultNoteFolder =
        notesModel->main_node();
        if (defaultNoteFolder) {
          notesModel->AddNote(defaultNoteFolder,
                       defaultNoteFolder->children().size(),
                       base::UTF8ToUTF16(value->GetString()),
                       GURL(), base::UTF8ToUTF16(value->GetString()));
        }
      }
    };

    web::WebFrame* main_frame =
        [self getCurrentWebState]->GetPageWorldWebFramesManager()
                                ->GetMainWebFrame();
    if (main_frame) {
      main_frame->ExecuteJavaScript(
                   base::SysNSStringToUTF16(@"window.getSelection().toString()"),
                   base::BindOnce(javascript_completion));
    }
  }
}
#endif

- (void)dismissNoteController{
    [_noteInteractionController dismissNoteModalControllerAnimated:NO];
    [_noteInteractionController dismissSnackbar];
}

- (void)shutdownNoteController{
    [_noteInteractionController shutdown];
    _noteInteractionController = nil;
}

- (void)dismissNoteSnackbar{
    [_noteInteractionController dismissSnackbar];
}

- (void)dismissNoteModalControllerAnimated{
[_noteInteractionController dismissNoteModalControllerAnimated:YES];
}

- (void)setNoteInteractionController:(NoteInteractionController *)item {
    objc_setAssociatedObject(self, &key,
      item, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

- (NoteInteractionController*)noteInteractionController {
    return objc_getAssociatedObject(self, &key);
}

@end
