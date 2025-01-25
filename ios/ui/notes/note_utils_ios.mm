// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/notes/note_utils_ios.h"

#import <memory>
#import <stdint.h>
#import <vector>

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/check.h"
#import "base/hash/hash.h"
#import "base/i18n/string_compare.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/notes/note_node.h"
#import "components/notes/notes_model.h"
#import "components/notes/notes_model.h"
#import "components/query_parser/query_parser.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "third_party/skia/include/core/SkColor.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/models/tree_node_iterator.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using vivaldi::NoteNode;
using vivaldi::NotesModel;

namespace  {
  const int MAXIMUM_TITLE_LENGTH = 128;
}

namespace note_utils_ios {

NSString* const kNotesSnackbarCategory = @"NotesSnackbarCategory";

std::optional<NodeSet> FindNodesByIds(NotesModel* model,
                                       const std::set<int64_t>& ids) {
  DCHECK(model);
  NodeSet nodes;
  ui::TreeNodeIterator<const NoteNode> iterator(model->root_node());
  while (iterator.has_next()) {
    const NoteNode* node = iterator.Next();
    if (ids.find(node->id()) == ids.end())
      continue;

    nodes.insert(node);
    if (ids.size() == nodes.size())
      break;
  }

  if (ids.size() != nodes.size())
    return std::nullopt;

  return nodes;
}

const NoteNode* FindNodeById(vivaldi::NotesModel* model, int64_t id) {
  DCHECK(model);
  ui::TreeNodeIterator<const NoteNode> iterator(model->root_node());
  while (iterator.has_next()) {
    const NoteNode* node = iterator.Next();
    if (node->id() == id)
      return node;
  }

  return nullptr;
}

const NoteNode* FindFolderById(vivaldi::NotesModel* model,
                                   int64_t id) {
  const NoteNode* node = FindNodeById(model, id);
  return node && node->is_folder() ? node : nullptr;
}

NSString* NormalizeTitle(const NSString* title) {
  NSArray* lines = [title componentsSeparatedByString:@"\n"];
  if ([lines count] <= 0) {
    return [title mutableCopy];
  }
  NSString* normalizeTitle = [[lines objectAtIndex:0]
                              stringByTrimmingCharactersInSet:
                                [NSCharacterSet whitespaceCharacterSet]];
  // Convert all multiples spaces into singles
  normalizeTitle = [normalizeTitle
                    stringByReplacingOccurrencesOfString:@" {2,}"
                    withString:@""
                    options:NSRegularExpressionSearch
                    range:NSMakeRange(0, normalizeTitle.length)];
  // Normalizes the header level for simpicity
  normalizeTitle = [normalizeTitle
                    stringByReplacingOccurrencesOfString:@"#"
                    withString:@""
                    options:NSRegularExpressionSearch
                    range:NSMakeRange(0, normalizeTitle.length)];

  if (normalizeTitle.length > MAXIMUM_TITLE_LENGTH) {
    normalizeTitle = [normalizeTitle substringToIndex:MAXIMUM_TITLE_LENGTH];
  }
  return normalizeTitle;
}

NSString* TitleForNoteNode(const NoteNode* node) {
  NSString* title;
  if (node->is_main()) {
    title = l10n_util::GetNSString(IDS_VIVALDI_NOTE_NEW_NOTES_BAR_TITLE);
  } else if (node->is_other()) {
    title = l10n_util::GetNSString(IDS_VIVALDI_NOTE_BAR_OTHER_FOLDER_NAME);
  } else if (node->is_trash()) {
    title = l10n_util::GetNSString(IDS_VIVALDI_NOTE_FOLDER_TRASH);
  } else {
    title = base::SysUTF16ToNSString(node->GetTitle());
  }

  if (!node->GetTitle().empty()) {
    title = base::SysUTF16ToNSString(node->GetTitle());
  } else {
    title = base::SysUTF16ToNSString(node->GetContent());
  }

  title = NormalizeTitle(title);

  // Assign a default note name if it is at top level.
  if (node->is_root() && ![title length])
    title = l10n_util::GetNSString(IDS_VIVALDI_SYNC_DATATYPE_NOTES);

  return title;
}

NSString* subtitleForNoteNode(const NoteNode* node) {
  int childCount = node->GetTotalNodeCount() - 1;
  NSString* subtitle;
  if (childCount == 0) {
    subtitle = l10n_util::GetNSString(IDS_VIVALDI_NOTE_NO_ITEM_COUNT);
  } else if (childCount == 1) {
    subtitle = l10n_util::GetNSString(IDS_VIVALDI_NOTE_ONE_ITEM_COUNT);
  } else {
    NSString* childCountString = [NSString stringWithFormat:@"%d", childCount];
    subtitle =
        l10n_util::GetNSStringF(IDS_VIVALDI_NOTE_ITEM_COUNT,
                                base::SysNSStringToUTF16(childCountString));
  }
  return subtitle;
}

NSDate* createdAtForNoteNode(const vivaldi::NoteNode* node) {
  return node->GetCreationTime().ToNSDate();
}

NSDate* lastModificationTimeForNoteNode(const vivaldi::NoteNode* node) {
  return node->GetLastModificationTime().ToNSDate();
}

#pragma mark - Updating Notes

// Deletes all subnodes of |node|, including |node|, that are in |notes|.
void DeleteNotes(const std::set<const NoteNode*>& notes,
                     vivaldi::NotesModel* model,
                     const NoteNode* node) {
  // Delete children in reverse order, so that the index remains valid.
  for (size_t i = node->children().size(); i > 0; --i) {
    DeleteNotes(notes, model, node->children()[i - 1].get());
  }
  if (notes.find(node) != notes.end())
    model->Remove(node, FROM_HERE);
}

// Creates a toast which will undo the changes made to the note model if
// the user presses the undo button, and the UndoManagerWrapper allows the undo
// to go through.
MDCSnackbarMessage* CreateToastWithWrapper(NSString* text) {
  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  MDCSnackbarMessage* message = [MDCSnackbarMessage messageWithText:text];
  message.category = kNotesSnackbarCategory;
  return message;
}

MDCSnackbarMessage* CreateOrUpdateNoteWithToast(
    const NoteNode* node,
    NSString* content,
    const GURL& url,
    const NoteNode* folder,
    vivaldi::NotesModel* note_model,
     ProfileIOS* profile) {
  std::u16string contentString = base::SysNSStringToUTF16(content);

  // Save the note information.
  if (!node) {
    // Normalize title first from content
    std::u16string title = base::SysNSStringToUTF16(NormalizeTitle(content));
    // Create a new note.
    node = note_model->AddNote(folder, folder->children().size(),
                                  title, url, contentString);
  } else {  // Update the information.
    std::u16string nodeTitle = base::SysNSStringToUTF16(TitleForNoteNode(node));
    note_model->SetContent(node, contentString);
    note_model->SetTitle(node, nodeTitle);
    note_model->SetURL(node, url);

    DCHECK(folder);
    DCHECK(!folder->HasAncestor(node));
    if (node->parent() != folder) {
      note_model->Move(node, folder, folder->children().size());
    }
    DCHECK(node->parent() == folder);
  }

  NSString* text =
      l10n_util::GetNSString((node) ? IDS_VIVALDI_NOTE_NEW_NOTE_UPDATED
                                    : IDS_VIVALDI_NOTE_NEW_NOTE_CREATED);
  return CreateToastWithWrapper(text);
}

MDCSnackbarMessage* CreateNoteAtPositionWithToast(
    NSString* title,
    const GURL& url,
    const vivaldi::NoteNode* folder,
    int position,
    vivaldi::NotesModel* note_model,
    ProfileIOS* profile) {
  std::u16string titleString = base::SysNSStringToUTF16(title);

  const vivaldi::NoteNode* node = note_model->AddNote(
      folder, folder->children().size(), titleString, url, titleString);
  note_model->Move(node, folder, position);

  NSString* text =
      l10n_util::GetNSString(IDS_VIVALDI_NOTE_NEW_NOTE_CREATED);
  return CreateToastWithWrapper(text);
}

MDCSnackbarMessage* UpdateNotePositionWithToast(
    const vivaldi::NoteNode* node,
    const vivaldi::NoteNode* folder,
    size_t position,
    vivaldi::NotesModel* note_model,
    ProfileIOS* profile) {
  DCHECK(node);
  DCHECK(folder);
  DCHECK(!folder->HasAncestor(node));
  // Early return if node is not valid.
  if (!node && !folder) {
    return nil;
  }

  size_t old_index = node->parent()->GetIndexOf(node).value();
  // Early return if no change in position.
  if (node->parent() == folder && old_index == position) {
    return nil;
  }
  note_model->Move(node, folder, position);
  NSString* text =
      l10n_util::GetNSString(IDS_VIVALDI_NOTE_NEW_NOTE_UPDATED);
  return CreateToastWithWrapper(text);
}

void DeleteNotes(const std::set<const NoteNode*>& notes,
                     vivaldi::NotesModel* model) {
  DCHECK(model->loaded());
  DeleteNotes(notes, model, model->root_node());
}

MDCSnackbarMessage* DeleteNotesWithToast(
    const std::set<const NoteNode*>& nodes,
    vivaldi::NotesModel* model,
    ProfileIOS* profile) {
  size_t nodeCount = nodes.size();
  DCHECK_GT(nodeCount, 0u);

  // Delete the selected notes.
  note_utils_ios::DeleteNotes(nodes, model);

  NSString* text = nil;

  if (nodeCount == 1) {
    text = l10n_util::GetNSString(IDS_VIVALDI_NOTE_NEW_SINGLE_NOTE_DELETE);
  } else {
    NSString* countString = [NSString stringWithFormat:@"%zu", nodeCount];
    text =
        l10n_util::GetNSStringF(IDS_VIVALDI_NOTE_NEW_MULTIPLE_NOTE_DELETE,
                                base::SysNSStringToUTF16(countString));
  }

  return CreateToastWithWrapper(text);
}

bool MoveNotes(const std::set<const NoteNode*>& notes,
                   vivaldi::NotesModel* model,
                   const NoteNode* folder) {
  bool didPerformMove = false;

  // Calling Move() on the model will triger observer methods to fire, one of
  // them may modify the passed in |notes|. To protect against this scenario
  // a copy of the set is made first.
  const std::set<const NoteNode*> notes_copy(notes);
  for (const NoteNode* node : notes_copy) {
    // The notes model can change under us at any time, so we can't make
    // any assumptions.
    if (folder->HasAncestor(node))
      continue;
    if (node->parent() != folder) {
      model->Move(node, folder, folder->children().size());
      didPerformMove = true;
    }
  }
  return didPerformMove;
}

MDCSnackbarMessage* MoveNotesWithToast(
    const std::set<const NoteNode*>& nodes,
    vivaldi::NotesModel* model,
    const NoteNode* folder,
    ProfileIOS* profile) {
  size_t nodeCount = nodes.size();
  DCHECK_GT(nodeCount, 0u);

  // Move the selected notes.
  bool didPerformMove = note_utils_ios::MoveNotes(nodes, model, folder);
  if (!didPerformMove)
    return nil;  // Don't return a snackbar when no real move as happened.

  NSString* text = nil;
  if (nodeCount == 1) {
    text = l10n_util::GetNSString(IDS_VIVALDI_NOTE_NEW_SINGLE_NOTE_MOVE);
  } else {
    NSString* countString = [NSString stringWithFormat:@"%zu", nodeCount];
    text = l10n_util::GetNSStringF(IDS_VIVALDI_NOTE_NEW_MULTIPLE_NOTE_MOVE,
                                   base::SysNSStringToUTF16(countString));
  }
  return CreateToastWithWrapper(text);
}

const NoteNode* defaultMoveFolder(
    const std::set<const NoteNode*>& notes,
    vivaldi::NotesModel* model) {
  if (notes.size() == 0)
    return model->main_node();
  const NoteNode* firstParent = (*(notes.begin()))->parent();
  for (const NoteNode* node : notes) {
    if (node->parent() != firstParent)
      return model->main_node();
  }

  return firstParent;
}

#pragma mark - Segregation of nodes by time.

NodesSection::NodesSection() {}

NodesSection::~NodesSection() {}

void segregateNodes(
    const NodeVector& vector,
    std::vector<std::unique_ptr<NodesSection>>& nodesSectionVector) {
  nodesSectionVector.clear();

  // Make a localized date formatter.
  NSDateFormatter* formatter = [[NSDateFormatter alloc] init];
  [formatter setDateFormat:@"MMMM yyyy"];
  // Segregate nodes by creation date.
  // Nodes that were created in the same month are grouped together.
  for (auto* node : vector) {
    @autoreleasepool {
      base::Time dateAdded = node->GetCreationTime();
      base::TimeDelta delta = dateAdded - base::Time::UnixEpoch();
      NSDate* date =
          [[NSDate alloc] initWithTimeIntervalSince1970:delta.InSeconds()];
      NSString* dateString = [formatter stringFromDate:date];
      const std::string timeRepresentation =
          base::SysNSStringToUTF8(dateString);

      BOOL found = NO;
      for (const auto& nodesSection : nodesSectionVector) {
        if (nodesSection->timeRepresentation == timeRepresentation) {
          nodesSection->vector.push_back(node);
          found = YES;
          break;
        }
      }

      if (found)
        continue;

      // No NodesSection found.
      auto nodesSection = std::make_unique<NodesSection>();
      nodesSection->time = dateAdded;
      nodesSection->timeRepresentation = timeRepresentation;
      nodesSection->vector.push_back(node);
      nodesSectionVector.push_back(std::move(nodesSection));
    }
  }

  // Sort the NodesSections.
  std::sort(nodesSectionVector.begin(), nodesSectionVector.end(),
            [](const std::unique_ptr<NodesSection>& n1,
               const std::unique_ptr<NodesSection>& n2) {
              return n1->time > n2->time;
            });

  // For each NodesSection, sort the nodes inside.
  for (const auto& nodesSection : nodesSectionVector) {
    std::sort(nodesSection->vector.begin(), nodesSection->vector.end(),
              [](const NoteNode* n1, const NoteNode* n2) {
                return n1->GetCreationTime() > n2->GetCreationTime();
              });
  }
}

#pragma mark - Useful note manipulation.

// Adds all children of |folder| that are not obstructed to |results|. They are
// placed immediately after |folder|, using a depth-first, then alphabetically
// ordering. |results| must contain |folder|.
void UpdateFoldersFromNode(const NoteNode* folder,
                           NodeVector* results,
                           const NodeSet& obstructions);
// Returns whether |folder| has an ancestor in any of the nodes in
// |noteNodes|.
bool FolderHasAncestorInNoteNodes(const NoteNode* folder,
                                      const NodeSet& noteNodes);
// Returns true if the node is not a folder, is not visible, or is an ancestor
// of any of the nodes in |obstructions|.
bool IsObstructed(const NoteNode* node, const NodeSet& obstructions);

namespace {
// Comparator used to sort notes. No folders are allowed.
class FolderNodeComparator {
 public:
  explicit FolderNodeComparator(icu::Collator* collator)
      : collator_(collator) {}

  // Returns true if |n1| precedes |n2|.
  bool operator()(const NoteNode* n1, const NoteNode* n2) {
    if (!collator_)
      return n1->GetTitle() < n2->GetTitle();
    return base::i18n::CompareString16WithCollator(*collator_, n1->GetTitle(),
                                                   n2->GetTitle()) == UCOL_LESS;
  }

 private:
  icu::Collator* collator_;
};
}  // namespace

bool FolderHasAncestorInNoteNodes(const NoteNode* folder,
                                      const NodeSet& noteNodes) {
  DCHECK(folder->is_folder());
  for (const NoteNode* node : noteNodes) {
    if (folder->HasAncestor(node))
      return true;
  }
  return false;
}

bool IsObstructed(const NoteNode* node, const NodeSet& obstructions) {
  if (!node->is_folder())
    return true;
  if (FolderHasAncestorInNoteNodes(node, obstructions))
    return true;
  return false;
}

void UpdateFoldersFromNode(const NoteNode* folder,
                           NodeVector* results,
                           const NodeSet& obstructions) {
  std::vector<const NoteNode*> directDescendants;
  for (const auto& subfolder : folder->children()) {
    if (!IsObstructed(subfolder.get(), obstructions))
      directDescendants.push_back(subfolder.get());
  }

  note_utils_ios::SortFolders(&directDescendants);

  auto it = std::find(results->begin(), results->end(), folder);
  DCHECK(it != results->end());
  ++it;
  results->insert(it, directDescendants.begin(), directDescendants.end());

  // Recursively perform the operation on each direct descendant.
  for (auto* node : directDescendants)
    UpdateFoldersFromNode(node, results, obstructions);
}

void SortFolders(NodeVector* vector) {
  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(icu::Collator::createInstance(error));
  if (U_FAILURE(error))
    collator.reset(NULL);
  std::sort(vector->begin(), vector->end(),
            FolderNodeComparator(collator.get()));
}

std::vector<const NoteNode*> PrimaryPermanentNodes(NotesModel* model) {
  DCHECK(model->loaded());
  std::vector<const NoteNode*> nodes;
  nodes.push_back(model->main_node());
  return nodes;
}

NodeVector VisibleNonDescendantNodes(const NodeSet& obstructions,
                                     vivaldi::NotesModel* model) {
  NodeVector results;
  NodeVector primaryNodes = PrimaryPermanentNodes(model);
  NodeVector filteredPrimaryNodes;
  for (auto* node : primaryNodes) {
    if (IsObstructed(node, obstructions))
      continue;
    filteredPrimaryNodes.push_back(node);
  }

  // Copy the results over.
  results = filteredPrimaryNodes;

  // Iterate over a static copy of the filtered, root folders.
  for (auto* node : filteredPrimaryNodes)
    UpdateFoldersFromNode(node, &results, obstructions);

  return results;
}

// Whether |vector1| contains only elements of |vector2| in the same order.
BOOL IsSubvectorOfNodes(const NodeVector& vector1, const NodeVector& vector2) {
  NodeVector::const_iterator it = vector2.begin();
  // Scan the first vector.
  for (const auto* node : vector1) {
    // Look for a match in the rest of the second vector. When found, advance
    // the iterator on vector2 to only focus on the remaining part of vector2,
    // so that ordering is verified.
    it = std::find(it, vector2.end(), node);
    if (it == vector2.end())
      return NO;
    // If found in vector2, advance the iterator so that the match is only
    // matched once.
    it++;
  }
  return YES;
}

// Returns the indices in |vector2| of the items in |vector2| that are not
// present in |vector1|.
// |vector1| MUST be a subvector of |vector2| in the sense of |IsSubvector|.
std::vector<NodeVector::size_type> MissingNodesIndices(
    const NodeVector& vector1,
    const NodeVector& vector2) {
  DCHECK(IsSubvectorOfNodes(vector1, vector2))
      << "Can't compute missing nodes between nodes among which the first is "
         "not a subvector of the second.";

  std::vector<NodeVector::size_type> missingNodesIndices;
  // Keep an iterator on vector1.
  NodeVector::const_iterator it1 = vector1.begin();
  // Scan vector2, looking for vector1 elements.
  for (NodeVector::size_type i2 = 0; i2 != vector2.size(); i2++) {
    // When vector1 has been fully traversed, all remaining elements of vector2
    // are to be added to the missing nodes.
    // Otherwise, while the element of vector2 is not equal to the element the
    // iterator on vector1 is pointing to, add vector2 elements to the missing
    // nodes.
    if (it1 == vector1.end() || vector2[i2] != *it1) {
      missingNodesIndices.push_back(i2);
    } else {
      // When there is a match between vector2 and vector1, advance the iterator
      // of vector1.
      it1++;
    }
  }
  return missingNodesIndices;
}

#pragma mark - Cache position in table view.

NSArray* CreateNotePath(vivaldi::NotesModel* model, int64_t folderId) {
  // Create an array with root node id, if folderId == root node.
  if (model->root_node()->id() == folderId) {
    return @[ [NSNumber numberWithLongLong:model->root_node()->id()] ];
  }

  const NoteNode* note = FindFolderById(model, folderId);
  if (!note)
    return nil;

  NSMutableArray* notePath = [NSMutableArray array];
  [notePath addObject:[NSNumber numberWithLongLong:folderId]];
  while (model->root_node()->id() != note->id()) {
    note = note->parent();
    DCHECK(note);
    [notePath addObject:[NSNumber numberWithLongLong:note->id()]];
  }
  return [[notePath reverseObjectEnumerator] allObjects];
}

}  // namespace note_utils_ios
