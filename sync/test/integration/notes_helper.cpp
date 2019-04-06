// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/integration/notes_helper.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "notes/notes_factory.h"
#include "ui/base/models/tree_node_iterator.h"

using vivaldi::NotesModelFactory;

namespace {

// Returns the number of nodes of node type |node_type| in |model| whose
// titles match the string |title|.
int CountNodesWithTitlesMatching(Notes_Model* model,
                                 Notes_Node::Type node_type,
                                 const base::string16& title) {
  ui::TreeNodeIterator<const Notes_Node> iterator(model->root_node());
  // Walk through the model tree looking for notes nodes of node type
  // |node_type| whose titles match |title|.
  int count = 0;
  while (iterator.has_next()) {
    const Notes_Node* node = iterator.Next();
    if ((node->type() == node_type) && (node->GetTitle() == title))
      ++count;
  }
  return count;
}

// Does a deep comparison of Notes_Node fields in |model_a| and |model_b|.
// Returns true if they are all equal.
bool NodesMatch(const Notes_Node* node_a, const Notes_Node* node_b) {
  if (node_a == NULL || node_b == NULL)
    return node_a == node_b;
  if (node_a->is_folder() != node_b->is_folder()) {
    LOG(ERROR) << "Cannot compare folder with notes \"" << node_a->GetTitle()
               << "\" vs. \"" << node_b->GetTitle() << "\"";
    return false;
  }
  if (node_a->GetTitle() != node_b->GetTitle()) {
    LOG(ERROR) << "Title mismatch: " << node_a->GetTitle() << " vs. "
               << node_b->GetTitle();
    return false;
  }
  if (node_a->GetURL() != node_b->GetURL()) {
    LOG(ERROR) << "URL mismatch: " << node_a->GetURL() << " vs. "
               << node_b->GetURL();
    return false;
  }
  if (node_a->GetContent() != node_b->GetContent()) {
    LOG(ERROR) << "Content mismatch: " << node_a->GetContent() << " vs. "
               << node_b->GetContent();
    return false;
  }
  if (node_a->parent() == NULL && node_b->parent() == NULL)
    return true;
  if (node_a->parent() == NULL && node_b->parent() != NULL) {
    LOG(ERROR) << "Parent Mismatch (node_a): " << node_a->GetTitle() << " vs. "
               << node_b->GetTitle();
    return false;
  }
  if (node_a->parent() != NULL && node_b->parent() == NULL) {
    LOG(ERROR) << "Parent Mismatch (node_b): " << node_a->GetTitle() << " vs. "
               << node_b->GetTitle();
    return false;
  }
  DCHECK(node_a->parent());
  DCHECK(node_b->parent());
  if (node_a->parent()->GetIndexOf(node_a) !=
      node_b->parent()->GetIndexOf(node_b)) {
    LOG(ERROR) << "Index mismatch: " << node_a->parent()->GetIndexOf(node_a)
               << " vs. " << node_b->parent()->GetIndexOf(node_b);
    return false;
  }
  return true;
}

// Checks if the hierarchies in |model_a| and |model_b| are equivalent in
// terms of the data model and favicon. Returns true if they both match.
// Note: Some peripheral fields like creation times are allowed to mismatch.
bool NotesModelsMatch(Notes_Model* model_a, Notes_Model* model_b) {
  bool ret_val = true;
  ui::TreeNodeIterator<const Notes_Node> iterator_a(model_a->root_node());
  ui::TreeNodeIterator<const Notes_Node> iterator_b(model_b->root_node());
  while (iterator_a.has_next()) {
    const Notes_Node* node_a = iterator_a.Next();
    if (!iterator_b.has_next()) {
      LOG(ERROR) << "Models do not match.";
      return false;
    }
    const Notes_Node* node_b = iterator_b.Next();
    ret_val = ret_val && NodesMatch(node_a, node_b);
  }
  ret_val = ret_val && (!iterator_b.has_next());
  return ret_val;
}

// Finds the node in the verifier notes model that corresponds to
// |foreign_node| in |foreign_model| and stores its address in |result|.
void FindNodeInVerifier(Notes_Model* foreign_model,
                        const Notes_Node* foreign_node,
                        const Notes_Node** result) {
  // Climb the tree.
  std::stack<int> path;
  const Notes_Node* walker = foreign_node;
  while (walker != foreign_model->root_node()) {
    path.push(walker->parent()->GetIndexOf(walker));
    walker = walker->parent();
  }

  // Swing over to the other tree.
  walker = notes_helper::GetVerifierNotesModel()->root_node();

  // Climb down.
  while (!path.empty()) {
    ASSERT_TRUE(walker->is_folder());
    ASSERT_LT(path.top(), walker->child_count());
    walker = walker->GetChild(path.top());
    path.pop();
  }

  ASSERT_TRUE(NodesMatch(foreign_node, walker));
  *result = walker;
}

}  // namespace

namespace notes_helper {

Notes_Model* GetNotesModel(int index) {
  return NotesModelFactory::GetForBrowserContext(
      sync_datatype_helper::test()->GetProfile(index));
}

const Notes_Node* GetNotesTopNode(int index) {
  return GetNotesModel(index)->main_node();
}

Notes_Model* GetVerifierNotesModel() {
  return NotesModelFactory::GetForBrowserContext(
      sync_datatype_helper::test()->verifier());
}

const Notes_Node* AddNote(int profile,
                          const std::string& title,
                          const GURL& url) {
  return AddNote(profile, GetNotesTopNode(profile), 0, title, url,
                 CreateAutoIndexedContent());
}

const Notes_Node* AddNote(int profile,
                          int index,
                          const std::string& title,
                          const GURL& url) {
  return AddNote(profile, GetNotesTopNode(profile), index, title, url,
                 CreateAutoIndexedContent());
}

const Notes_Node* AddNote(int profile,
                          const Notes_Node* parent,
                          int index,
                          const std::string& title,
                          const GURL& url) {
  return AddNote(profile, parent, index, title, url,
                 CreateAutoIndexedContent());
}

const Notes_Node* AddNote(int profile,
                          const Notes_Node* parent,
                          int index,
                          const std::string& title,
                          const GURL& url,
                          const std::string& content) {
  Notes_Model* model = GetNotesModel(profile);
  if (GetNotesNodeByID(model, parent->id()) != parent) {
    LOG(ERROR) << "Node " << parent->GetTitle() << " does not belong to "
               << "Profile " << profile;
    return NULL;
  }
  const Notes_Node* result = model->AddNote(
      parent, index, base::UTF8ToUTF16(title), url, base::UTF8ToUTF16(content));
  if (!result) {
    LOG(ERROR) << "Could not add notes " << title << " to Profile " << profile;
    return NULL;
  }
  if (sync_datatype_helper::test()->use_verifier()) {
    const Notes_Node* v_parent = NULL;
    FindNodeInVerifier(model, parent, &v_parent);
    const Notes_Node* v_node = GetVerifierNotesModel()->AddNote(
        v_parent, index, base::UTF8ToUTF16(title), url,
        base::UTF8ToUTF16(content));
    if (!v_node) {
      LOG(ERROR) << "Could not add notes " << title << " to the verifier";
      return NULL;
    }
    EXPECT_TRUE(NodesMatch(v_node, result));
  }
  return result;
}

const Notes_Node* AddFolder(int profile, const std::string& title) {
  return AddFolder(profile, GetNotesTopNode(profile), 0, title);
}

const Notes_Node* AddFolder(int profile, int index, const std::string& title) {
  return AddFolder(profile, GetNotesTopNode(profile), index, title);
}

const Notes_Node* AddFolder(int profile,
                            const Notes_Node* parent,
                            int index,
                            const std::string& title) {
  Notes_Model* model = GetNotesModel(profile);
  EXPECT_TRUE(model);
  EXPECT_TRUE(parent);
  if (GetNotesNodeByID(model, parent->id()) != parent) {
    LOG(ERROR) << "Node " << parent->GetTitle() << " does not belong to "
               << "Profile " << profile;
    return NULL;
  }
  const Notes_Node* result =
      model->AddFolder(parent, index, base::UTF8ToUTF16(title));
  EXPECT_TRUE(result);
  if (!result) {
    LOG(ERROR) << "Could not add folder " << title << " to Profile " << profile;
    return NULL;
  }
  EXPECT_TRUE(result->parent() == parent);
  if (sync_datatype_helper::test()->use_verifier()) {
    const Notes_Node* v_parent = NULL;
    FindNodeInVerifier(model, parent, &v_parent);
    DCHECK(v_parent);
    const Notes_Node* v_node = GetVerifierNotesModel()->AddFolder(
        v_parent, index, base::UTF8ToUTF16(title));
    if (!v_node) {
      LOG(ERROR) << "Could not add folder " << title << " to the verifier";
      return NULL;
    }
    EXPECT_TRUE(NodesMatch(v_node, result));
  }
  return result;
}

void SetTitle(int profile,
              const Notes_Node* node,
              const std::string& new_title) {
  Notes_Model* model = GetNotesModel(profile);
  ASSERT_EQ(GetNotesNodeByID(model, node->id()), node)
      << "Node " << node->GetTitle() << " does not belong to "
      << "Profile " << profile;
  if (sync_datatype_helper::test()->use_verifier()) {
    const Notes_Node* v_node = NULL;
    FindNodeInVerifier(model, node, &v_node);
    GetVerifierNotesModel()->SetTitle(v_node, base::UTF8ToUTF16(new_title));
  }
  model->SetTitle(node, base::UTF8ToUTF16(new_title));
}

const Notes_Node* SetURL(int profile,
                         const Notes_Node* node,
                         const GURL& new_url) {
  Notes_Model* model = GetNotesModel(profile);
  if (GetNotesNodeByID(model, node->id()) != node) {
    LOG(ERROR) << "Node " << node->GetTitle() << " does not belong to "
               << "Profile " << profile;
    return NULL;
  }
  if (sync_datatype_helper::test()->use_verifier()) {
    const Notes_Node* v_node = NULL;
    FindNodeInVerifier(model, node, &v_node);
    if (v_node->is_note())
      GetVerifierNotesModel()->SetURL(v_node, new_url);
  }
  if (node->is_note())
    model->SetURL(node, new_url);
  return node;
}

void Move(int profile,
          const Notes_Node* node,
          const Notes_Node* new_parent,
          int index) {
  Notes_Model* model = GetNotesModel(profile);
  ASSERT_EQ(GetNotesNodeByID(model, node->id()), node)
      << "Node " << node->GetTitle() << " does not belong to "
      << "Profile " << profile;
  if (sync_datatype_helper::test()->use_verifier()) {
    const Notes_Node* v_new_parent = NULL;
    const Notes_Node* v_node = NULL;
    FindNodeInVerifier(model, new_parent, &v_new_parent);
    FindNodeInVerifier(model, node, &v_node);
    GetVerifierNotesModel()->Move(v_node, v_new_parent, index);
  }
  model->Move(node, new_parent, index);
}

void Remove(int profile, const Notes_Node* parent, int index) {
  Notes_Model* model = GetNotesModel(profile);
  ASSERT_EQ(GetNotesNodeByID(model, parent->id()), parent)
      << "Node " << parent->GetTitle() << " does not belong to "
      << "Profile " << profile;
  if (sync_datatype_helper::test()->use_verifier()) {
    const Notes_Node* v_parent = NULL;
    FindNodeInVerifier(model, parent, &v_parent);
    ASSERT_TRUE(NodesMatch(parent->GetChild(index), v_parent->GetChild(index)));
    GetVerifierNotesModel()->Remove(v_parent->GetChild(index));
  }
  model->Remove(parent->GetChild(index));
}

void RemoveAll(int profile) {
  if (sync_datatype_helper::test()->use_verifier()) {
    const Notes_Node* root_node = GetVerifierNotesModel()->root_node();
    for (int i = 0; i < root_node->child_count(); ++i) {
      const Notes_Node* permanent_node = root_node->GetChild(i);
      for (int j = permanent_node->child_count() - 1; j >= 0; --j) {
        GetVerifierNotesModel()->Remove(permanent_node->GetChild(j));
      }
    }
  }
  GetNotesModel(profile)->RemoveAllUserNotes();
}

void SortChildren(int profile, const Notes_Node* parent) {
  Notes_Model* model = GetNotesModel(profile);
  ASSERT_EQ(GetNotesNodeByID(model, parent->id()), parent)
      << "Node " << parent->GetTitle() << " does not belong to "
      << "Profile " << profile;
  if (sync_datatype_helper::test()->use_verifier()) {
    const Notes_Node* v_parent = NULL;
    FindNodeInVerifier(model, parent, &v_parent);
    GetVerifierNotesModel()->SortChildren(v_parent);
  }
  model->SortChildren(parent);
}

void ReverseChildOrder(int profile, const Notes_Node* parent) {
  ASSERT_EQ(GetNotesNodeByID(GetNotesModel(profile), parent->id()), parent)
      << "Node " << parent->GetTitle() << " does not belong to "
      << "Profile " << profile;
  int child_count = parent->child_count();
  if (child_count <= 0)
    return;
  for (int index = 0; index < child_count; ++index) {
    Move(profile, parent->GetChild(index), parent, child_count - index);
  }
}

bool ModelMatchesVerifier(int profile) {
  if (!sync_datatype_helper::test()->use_verifier()) {
    LOG(ERROR) << "Illegal to call ModelMatchesVerifier() after "
               << "DisableVerifier(). Use ModelsMatch() instead.";
    return false;
  }
  return NotesModelsMatch(GetVerifierNotesModel(), GetNotesModel(profile));
}

bool AllModelsMatchVerifier() {
  for (int i = 0; i < sync_datatype_helper::test()->num_clients(); ++i) {
    if (!ModelMatchesVerifier(i)) {
      LOG(ERROR) << "Model " << i << " does not match the verifier.";
      return false;
    }
  }
  return true;
}

bool ModelsMatch(int profile_a, int profile_b) {
  return NotesModelsMatch(GetNotesModel(profile_a), GetNotesModel(profile_b));
}

bool AllModelsMatch() {
  for (int i = 1; i < sync_datatype_helper::test()->num_clients(); ++i) {
    if (!ModelsMatch(0, i)) {
      LOG(ERROR) << "Model " << i << " does not match Model 0.";
      return false;
    }
  }
  return true;
}

namespace {

// Helper class used in the implementation of AwaitAllModelsMatch.
class AllModelsMatchChecker : public MultiClientStatusChangeChecker {
 public:
  AllModelsMatchChecker();
  ~AllModelsMatchChecker() override;

  bool IsExitConditionSatisfied() override;
  std::string GetDebugMessage() const override;
};

AllModelsMatchChecker::AllModelsMatchChecker()
    : MultiClientStatusChangeChecker(
          sync_datatype_helper::test()->GetSyncServices()) {}

AllModelsMatchChecker::~AllModelsMatchChecker() {}

bool AllModelsMatchChecker::IsExitConditionSatisfied() {
  return AllModelsMatch();
}

std::string AllModelsMatchChecker::GetDebugMessage() const {
  return "Waiting for matching models";
}

}  //  namespace

bool AwaitAllModelsMatch() {
  AllModelsMatchChecker checker;
  checker.Wait();
  return !checker.TimedOut();
}

bool ContainsDuplicateNotes(int profile) {
  ui::TreeNodeIterator<const Notes_Node> iterator(
      GetNotesModel(profile)->root_node());
  while (iterator.has_next()) {
    const Notes_Node* node = iterator.Next();
    if (node->is_folder())
      continue;
    std::vector<const Notes_Node*> nodes;
    GetNotesModel(profile)->GetNodesByURL(node->GetURL(), &nodes);
    EXPECT_GE(nodes.size(), 1ul);
    for (std::vector<const Notes_Node*>::const_iterator it = nodes.begin();
         it != nodes.end(); ++it) {
      if (node->id() != (*it)->id() && node->parent() == (*it)->parent() &&
          node->GetURL() == (*it)->GetURL() &&
          node->GetTitle() == (*it)->GetTitle() &&
          node->GetContent() == (*it)->GetContent()) {
        return true;
      }
    }
  }
  return false;
}

bool HasNodeWithURL(int profile, const GURL& url) {
  std::vector<const Notes_Node*> nodes;
  GetNotesModel(profile)->GetNodesByURL(url, &nodes);
  return !nodes.empty();
}

const Notes_Node* GetUniqueNodeByURL(int profile, const GURL& url) {
  std::vector<const Notes_Node*> nodes;
  GetNotesModel(profile)->GetNodesByURL(url, &nodes);
  EXPECT_EQ(1U, nodes.size());
  if (nodes.empty())
    return NULL;
  return nodes[0];
}

int CountNotesWithTitlesMatching(int profile, const std::string& title) {
  return CountNodesWithTitlesMatching(GetNotesModel(profile), Notes_Node::NOTE,
                                      base::UTF8ToUTF16(title));
}

int CountFoldersWithTitlesMatching(int profile, const std::string& title) {
  return CountNodesWithTitlesMatching(
      GetNotesModel(profile), Notes_Node::FOLDER, base::UTF8ToUTF16(title));
}

}  // namespace notes_helper
