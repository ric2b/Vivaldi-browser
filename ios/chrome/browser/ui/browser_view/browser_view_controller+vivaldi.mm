// Copyright 2022 Vivaldi Technologies. All rights reserved.


#import "ios/chrome/browser/ui/browser_view/browser_view_controller+vivaldi.h"

#import <objc/runtime.h>
#import <MaterialComponents/MaterialSnackbar.h>
#include <memory>

#import "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/notes/note_interaction_controller.h"
#include "ios/notes/notes_factory.h"
#import "ios/web/public/web_state.h"
#include "notes/note_node.h"
#include "notes/notes_model.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/mobile_common/grit/vivaldi_mobile_common_native_strings.h"

namespace {
const void* const key = &key;

}

@implementation BrowserViewController(Vivaldi)

NoteInteractionController* _noteInteractionController;

// Initializes the note interaction controller if not already initialized.
- (void)showNotesManager:(Browser*)browser
        parentController:(BrowserViewController*)bvc{
    if (_noteInteractionController) {
        [_noteInteractionController presentNotes];
        return;
   }
   _noteInteractionController =
      [[NoteInteractionController alloc] initWithBrowser:browser
                                            parentController:bvc];
   [_noteInteractionController presentNotes];
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
    [self getCurrentWebState]->ExecuteJavaScript(
         base::SysNSStringToUTF16(@"window.getSelection().toString()"),
         base::BindOnce(javascript_completion));
    NSString* text =
        l10n_util::GetNSString(IDS_VIVALDI_NOTE_SNACKBAR_MESSAGE);
    MDCSnackbarMessage* message = [MDCSnackbarMessage messageWithText:text];
    message.accessibilityLabel = text;
    message.duration = 2.0;
    // TODO [[self getCommandDispatcher] showSnackbarMessage:[self getSnackbarCategory]];
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
