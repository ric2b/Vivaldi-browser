// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_INTEGRATION_NOTES_HELPER_H_
#define SYNC_TEST_INTEGRATION_NOTES_HELPER_H_

#include "notes/notes_model.h"
#include "notes/notesnode.h"
#include "notes/tests/notes_contenthelper.h"

class GURL;
using vivaldi::Notes_Model;
using vivaldi::Notes_Node;

namespace notes_helper {

// Used to access the notes model within a particular sync profile.
Notes_Model* GetNotesModel(int index) WARN_UNUSED_RESULT;

// Used to access the "Synced Notes" node within a particular sync profile.
const Notes_Node* GetNotesTopNode(int index) WARN_UNUSED_RESULT;

// Used to access the notes within the verifier sync profile.
Notes_Model* GetVerifierNotesModel() WARN_UNUSED_RESULT;

// Adds a URL with address |url| and title |title| to the notes of
// profile |profile|. Returns a pointer to the node that was added.
const Notes_Node* AddNote(int profile,
                          const std::string& title,
                          const GURL& url) WARN_UNUSED_RESULT;

// Adds a URL with address |url| and title |title| to the notes of
// profile |profile| at position |index|. Returns a pointer to the node that
// was added.
const Notes_Node* AddNote(int profile,
                          int index,
                          const std::string& title,
                          const GURL& url) WARN_UNUSED_RESULT;

// Adds a URL with address |url| and title |title| under the node |parent| of
// profile |profile| at position |index|. Returns a pointer to the node that
// was added.
const Notes_Node* AddNote(int profile,
                          const Notes_Node* parent,
                          int index,
                          const std::string& title,
                          const GURL& url,
                          const std::string& content) WARN_UNUSED_RESULT;
const Notes_Node* AddNote(int profile,
                          const Notes_Node* parent,
                          int index,
                          const std::string& title,
                          const GURL& url) WARN_UNUSED_RESULT;

// Adds a folder named |title| to the notes profile |profile|.
// Returns a pointer to the folder that was added.
const Notes_Node* AddFolder(int profile,
                            const std::string& title) WARN_UNUSED_RESULT;

// Adds a folder named |title| to the notes of profile |profile| at
// position |index|. Returns a pointer to the folder that was added.
const Notes_Node* AddFolder(int profile,
                            int index,
                            const std::string& title) WARN_UNUSED_RESULT;

// Adds a folder named |title| to the node |parent| in the notes model of
// profile |profile| at position |index|. Returns a pointer to the node that
// was added.
const Notes_Node* AddFolder(int profile,
                            const Notes_Node* parent,
                            int index,
                            const std::string& title) WARN_UNUSED_RESULT;

// Changes the title of the node |node| in the notes model of profile
// |profile| to |new_title|.
void SetTitle(int profile,
              const Notes_Node* node,
              const std::string& new_title);

// Changes the url of the node |node| in the notes model of profile
// |profile| to |new_url|. Returns a pointer to the node with the changed url.
const Notes_Node* SetURL(int profile,
                         const Notes_Node* node,
                         const GURL& new_url) WARN_UNUSED_RESULT;

// Moves the node |node| in the notes model of profile |profile| so it ends
// up under the node |new_parent| at position |index|.
void Move(int profile,
          const Notes_Node* node,
          const Notes_Node* new_parent,
          int index);

// Removes the node in the notes model of profile |profile| under the node
// |parent| at position |index|.
void Remove(int profile, const Notes_Node* parent, int index);

// Removes all non-permanent nodes in the notes model of profile |profile|.
void RemoveAll(int profile);

// Sorts the children of the node |parent| in the notes model of profile
// |profile|.
void SortChildren(int profile, const Notes_Node* parent);

// Reverses the order of the children of the node |parent| in the notes
// model of profile |profile|.
void ReverseChildOrder(int profile, const Notes_Node* parent);

// Checks if the notes model of profile |profile| matches the verifier
// notes model. Returns true if they match.
bool ModelMatchesVerifier(int profile) WARN_UNUSED_RESULT;

// Checks if the notes models of all sync profiles match the verifier
// notes model. Returns true if they match.
bool AllModelsMatchVerifier() WARN_UNUSED_RESULT;

// Checks if the notes models of |profile_a| and |profile_b| match each
// other. Returns true if they match.
bool ModelsMatch(int profile_a, int profile_b) WARN_UNUSED_RESULT;

// Checks if the notes models of all sync profiles match each other. Does
// not compare them with the verifier notes model. Returns true if they
// match.
bool AllModelsMatch() WARN_UNUSED_RESULT;

// Check if the notess models of all sync profiles match each other, using
// AllModelsMatch. Returns true if notes models match and don't timeout
// while checking.
bool AwaitAllModelsMatch() WARN_UNUSED_RESULT;

// Checks if the notes model of profile |profile| contains any instances of
// two notes with the same URL under the same parent folder. Returns true
// if even one instance is found.
bool ContainsDuplicateNotes(int profile);

// Returns whether a node exists with the specified url.
bool HasNodeWithURL(int profile, const GURL& url);

// Gets the node in the notes model of profile |profile| that has the url
// |url|. Note: Only one instance of |url| is assumed to be present.
const Notes_Node* GetUniqueNodeByURL(int profile,
                                     const GURL& url) WARN_UNUSED_RESULT;

// Returns the number of notes in notes model of profile |profile|
// whose titles match the string |title|.
int CountNotesWithTitlesMatching(int profile,
                                 const std::string& title) WARN_UNUSED_RESULT;

// Returns the number of notes folders in the notes model of profile
// |profile| whose titles contain the query string |title|.
int CountFoldersWithTitlesMatching(int profile,
                                   const std::string& title) WARN_UNUSED_RESULT;

}  // namespace notes_helper

#endif  // SYNC_TEST_INTEGRATION_NOTES_HELPER_H_
