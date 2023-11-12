// Copyright 2022 Vivaldi Technologies. All rights reserved.


#import "ios/chrome/browser/ui/browser_view/browser_view_controller+vivaldi.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import <memory>
#import <objc/runtime.h>

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/notes/note_interaction_controller.h"
#import "ios/notes/notes_factory.h"
#import "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/web_state.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"
#import "notes/note_node.h"
#import "notes/notes_model.h"
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

- (void)onCopy:(UIMenuController*)sender {
    void (^javascript_completion)(const base::Value*) =
        ^(const base::Value* value) {
            vivaldi::NotesModel* notesModel =
                vivaldi::NotesModelFactory::GetForBrowserState(
                                            [self getBrowserState]);
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

    web::WebFrame* main_frame = web::GetMainFrame([self getCurrentWebState]);
    if (main_frame) {
      main_frame->ExecuteJavaScript(
        base::SysNSStringToUTF16(@"window.getSelection().toString()"),
        base::BindOnce(javascript_completion));
    }
    NSString* text =
        l10n_util::GetNSString(IDS_VIVALDI_NOTE_SNACKBAR_MESSAGE);
    MDCSnackbarMessage* message = [MDCSnackbarMessage messageWithText:text];
    message.accessibilityLabel = text;
    message.duration = 2.0;
}

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
