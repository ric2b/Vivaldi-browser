// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_INTEGRATION_NOTES_HELPER_H_
#define SYNC_TEST_INTEGRATION_NOTES_HELPER_H_

#include "components/notes/note_node.h"
#include "components/notes/notes_model.h"
#include "components/notes/tests/notes_contenthelper.h"

class GURL;
using vivaldi::NoteNode;
using vivaldi::NotesModel;

namespace notes_helper {

// Used to access the notes model within a particular sync profile.
[[nodiscard]] NotesModel* GetNotesModel(int index);

// Used to access the "Synced Notes" node within a particular sync profile.
[[nodiscard]] const NoteNode* GetNotesTopNode(int index);

// Used to access the notes within the verifier sync profile.
[[nodiscard]] NotesModel* GetVerifierNotesModel();

// Adds a URL with address |url| and content |content| to the notes of
// profile |profile|. Returns a pointer to the node that was added.
[[nodiscard]] const NoteNode* AddNote(int profile,
                                      const std::string& content,
                                      const GURL& url);

// Adds a URL with address |url|, title |title| and content |content| to the
// notes of profile |profile|. Returns a pointer to the node that was added.
[[nodiscard]] const NoteNode* AddNote(int profile,
                                      const std::string& title,
                                      const std::string& content,
                                      const GURL& url);

// Adds a URL with address |url| and content |content| to the notes of
// profile |profile| at position |index|. Returns a pointer to the node that
// was added.
[[nodiscard]] const NoteNode* AddNote(int profile,
                                      int index,
                                      const std::string& content,
                                      const GURL& url);

// Adds a URL with address |url|, title |title| and content |content| to the
// notes of profile |profile| at position |index|. Returns a pointer to the node
// that was added.
[[nodiscard]] const NoteNode* AddNote(int profile,
                                      int index,
                                      const std::string& content,
                                      const std::string& title,
                                      const GURL& url);

// Adds a URL with address |url| and content |content| under the node |parent|
// of profile |profile| at position |index|. Returns a pointer to the node that
// was added.
[[nodiscard]] const NoteNode* AddNote(int profile,
                                      const NoteNode* parent,
                                      int index,
                                      const std::string& content,
                                      const std::string& title,
                                      const GURL& url);
[[nodiscard]] const NoteNode* AddNote(int profile,
                                      const NoteNode* parent,
                                      int index,
                                      const std::string& content,
                                      const GURL& url);

// Adds a folder named |title| to the notes profile |profile|.
// Returns a pointer to the folder that was added.
[[nodiscard]] const NoteNode* AddFolder(int profile, const std::string& title);

// Adds a folder named |title| to the notes of profile |profile| at
// position |index|. Returns a pointer to the folder that was added.
[[nodiscard]] const NoteNode* AddFolder(int profile,
                                        int index,
                                        const std::string& title);

// Adds a folder named |title| to the node |parent| in the notes model of
// profile |profile| at position |index|. Returns a pointer to the node that
// was added.
[[nodiscard]] const NoteNode* AddFolder(int profile,
                                        const NoteNode* parent,
                                        int index,
                                        const std::string& title);

// Changes the title of the node |node| in the notes model of profile
// |profile| to |new_content|.
void SetContent(int profile,
                const NoteNode* node,
                const std::string& new_content);

// Changes the title of the node |node| in the notes model of profile
// |profile| to |new_title|.
void SetTitle(int profile, const NoteNode* node, const std::string& new_title);

// Changes the url of the node |node| in the notes model of profile
// |profile| to |new_url|. Returns a pointer to the node with the changed url.
[[nodiscard]] const NoteNode* SetURL(int profile,
                                     const NoteNode* node,
                                     const GURL& new_url);

// Moves the node |node| in the notes model of profile |profile| so it ends
// up under the node |new_parent| at position |index|.
void Move(int profile,
          const NoteNode* node,
          const NoteNode* new_parent,
          int index);

// Removes the node in the notes model of profile |profile| under the node
// |parent| at position |index|.
void Remove(int profile, const NoteNode* parent, int index);

// Removes all non-permanent nodes in the notes model of profile |profile|.
void RemoveAll(int profile);

// Sorts the children of the node |parent| in the notes model of profile
// |profile|.
void SortChildren(int profile, const NoteNode* parent);

// Reverses the order of the children of the node |parent| in the notes
// model of profile |profile|.
void ReverseChildOrder(int profile, const NoteNode* parent);

// Checks if the notes model of profile |profile| matches the verifier
// notes model. Returns true if they match.
[[nodiscard]] bool ModelMatchesVerifier(int profile);

// Checks if the notes models of |profile_a| and |profile_b| match each
// other. Returns true if they match.
[[nodiscard]] bool ModelsMatch(int profile_a, int profile_b);

// Checks if the notes models of all sync profiles match each other. Does
// not compare them with the verifier notes model. Returns true if they
// match.
[[nodiscard]] bool AllModelsMatch();

// Check if the notess models of all sync profiles match each other, using
// AllModelsMatch. Returns true if notes models match and don't timeout
// while checking.
[[nodiscard]] bool AwaitAllModelsMatch();

// Checks if the notes model of profile |profile| contains any instances of
// two notes with the same URL under the same parent folder. Returns true
// if even one instance is found.
bool ContainsDuplicateNotes(int profile);

// Returns whether a node exists with the specified url.
bool HasNodeWithURL(int profile, const GURL& url);

// Gets the node in the notes model of profile |profile| that has the url
// |url|. Note: Only one instance of |url| is assumed to be present.
[[nodiscard]] const NoteNode* GetUniqueNodeByURL(int profile, const GURL& url);

// Returns the number of notes in notes model of profile |profile|
// whose content match the string |content|.
[[nodiscard]] int CountNotesWithContentMatching(int profile,
                                                const std::string& content);

}  // namespace notes_helper

#endif  // SYNC_TEST_INTEGRATION_NOTES_HELPER_H_
