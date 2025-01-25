// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_NOTES_NOTE_UTILS_IOS_H_
#define IOS_UI_NOTES_NOTE_UTILS_IOS_H_

#import <UIKit/UIKit.h>

#import <memory>
#import <set>
#import <string>
#import <vector>

#import "base/time/time.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

class ProfileIOS;
class GURL;
@class MDCSnackbarMessage;

namespace vivaldi {
class NotesModel;
class NoteNode;
}  // namespace notes

namespace note_utils_ios {

typedef std::vector<const vivaldi::NoteNode*> NodeVector;
typedef std::set<const vivaldi::NoteNode*> NodeSet;

// Finds note nodes from passed in |ids|. The optional is only set if all
// the |ids| have been found.
std::optional<NodeSet> FindNodesByIds(vivaldi::NotesModel* model,
                                       const std::set<int64_t>& ids);

// Finds note node passed in |id|, in the |model|.
const vivaldi::NoteNode* FindNodeById(vivaldi::NotesModel* model,
                                            int64_t id);

// Finds note node passed in |id|, in the |model|. Returns null if the
// node is found but not a folder.
const vivaldi::NoteNode* FindFolderById(vivaldi::NotesModel* model,
                                              int64_t id);

// Returns the title string & normalizes it.
NSString* NormalizeTitle(const NSString* title);

// The iOS code is doing some munging of the note folder names in order
// to display a slighly different wording for the default folders.
NSString* TitleForNoteNode(const vivaldi::NoteNode* node);

// Returns the subtitle relevant to the note navigation ui.
NSString* subtitleForNoteNode(const vivaldi::NoteNode* node);

// Returns the noted create date and time from the node.
NSDate* createdAtForNoteNode(const vivaldi::NoteNode* node);

// Returns the noted last modified date and time from the node.
NSDate* lastModificationTimeForNoteNode(const vivaldi::NoteNode* node);

#pragma mark - Updating Notes

// A Snackbar helper to show toast messages.
MDCSnackbarMessage* CreateToastWithWrapper(NSString* text);

// Creates the note if |node| is NULL. Otherwise updates |node|.
// |folder| is the intended parent of |node|.
// Returns a snackbar with an undo action, returns nil if operation wasn't
// successful or there's nothing to undo.
// TODO(crbug.com/1099901): Refactor to include position and replace two
// functions below.
MDCSnackbarMessage* CreateOrUpdateNoteWithToast(
    const vivaldi::NoteNode* node,
    NSString* title,
    const GURL& url,
    const vivaldi::NoteNode* folder,
    vivaldi::NotesModel* note_model,
    ProfileIOS* profile);

// Creates a new note with |title|, |url|, at |position| under parent
// |folder|. Returns a snackbar with an undo action. Returns nil if operation
// failed or there's nothing to undo.
MDCSnackbarMessage* CreateNoteAtPositionWithToast(
    NSString* title,
    const GURL& url,
    const vivaldi::NoteNode* folder,
    int position,
    vivaldi::NotesModel* note_model,
    ProfileIOS* profile);

// Updates a note node position, and returns a snackbar with an undo action.
// Returns nil if the operation wasn't successful or there's nothing to undo.
MDCSnackbarMessage* UpdateNotePositionWithToast(
    const vivaldi::NoteNode* node,
    const vivaldi::NoteNode* folder,
    size_t position,
    vivaldi::NotesModel* note_model,
    ProfileIOS* profile);

// Deletes all notes in |model| that are in |notes|, and returns a
// snackbar with an undo action. Returns nil if the operation wasn't successful
// or there's nothing to undo.
MDCSnackbarMessage* DeleteNotesWithToast(
    const std::set<const vivaldi::NoteNode*>& notes,
    vivaldi::NotesModel* model,
    ProfileIOS* profile);

// Deletes all nodes in |notes|.
void DeleteNotes(const std::set<const vivaldi::NoteNode*>& notes,
                     vivaldi::NotesModel* model);

// Move all |notes| to the given |folder|, and returns a snackbar with an
// undo action. Returns nil if the operation wasn't successful or there's
// nothing to undo.
MDCSnackbarMessage* MoveNotesWithToast(
    const std::set<const vivaldi::NoteNode*>& notes,
    vivaldi::NotesModel* model,
    const vivaldi::NoteNode* folder,
    ProfileIOS* profile);

// Move all |notes| to the given |folder|.
// Returns whether this method actually moved notes (for example, only
// moving a folder to its parent will return |false|).
bool MoveNotes(const std::set<const vivaldi::NoteNode*>& notes,
                   vivaldi::NotesModel* model,
                   const vivaldi::NoteNode* folder);

// Category name for all notes related snackbars.
extern NSString* const kNotesSnackbarCategory;

// Returns the parent, if all the notes are siblings.
// Otherwise returns the mobile_node.
const vivaldi::NoteNode* defaultMoveFolder(
    const std::set<const vivaldi::NoteNode*>& notes,
    vivaldi::NotesModel* model);

#pragma mark - Segregation of nodes by time.

// A container for nodes which have been aggregated by some time property.
// e.g. (creation date) or (last access date).
class NodesSection {
 public:
  NodesSection();
  virtual ~NodesSection();

  // |vector| is sorted by the relevant time property.
  NodeVector vector;
  // A representative time of all nodes in vector.
  base::Time time;
  // A string representation of |time|.
  // e.g. (March 2014) or (4 March 2014).
  std::string timeRepresentation;
};

// Given the nodes in |vector|, segregates them by some time property into
// NodesSections.
// Automatically clears, populates and sorts |nodesSectionVector|.
// This method is not thread safe - it should only be called from the ui thread.
void segregateNodes(
    const NodeVector& vector,
    std::vector<std::unique_ptr<NodesSection>>& nodesSectionVector);

#pragma mark - Useful note manipulation.

// Sorts a vector full of folders by title.
void SortFolders(NodeVector* vector);

// Returns a vector of root level folders and all their folder descendants,
// sorted depth-first, then alphabetically. The returned nodes are visible, and
// are guaranteed to not be descendants of any nodes in |obstructions|.
NodeVector VisibleNonDescendantNodes(const NodeSet& obstructions,
                                     vivaldi::NotesModel* model);

// Whether |vector1| contains only elements of |vector2| in the same order.
BOOL IsSubvectorOfNodes(const NodeVector& vector1, const NodeVector& vector2);

// Returns the indices in |vector2| of the items in |vector2| that are not
// present in |vector1|.
// |vector1| MUST be a subvector of |vector2| in the sense of |IsSubvector|.
std::vector<NodeVector::size_type> MissingNodesIndices(
    const NodeVector& vector1,
    const NodeVector& vector2);

#pragma mark - Cache position in table view.

// Creates note path for |folderId| passed in. For eg: for folderId = 76,
// Root node(0) --> MobileNotes (3) --> Test1(76) will be returned as [0, 3,
// 76].
NSArray* CreateNotePath(vivaldi::NotesModel* model, int64_t folderId);

}  // namespace note_utils_ios

#endif  // IOS_UI_NOTES_NOTE_UTILS_IOS_H_
