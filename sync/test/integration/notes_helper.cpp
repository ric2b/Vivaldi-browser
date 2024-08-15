// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/integration/notes_helper.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/notes/notes_factory.h"
#include "ui/base/models/tree_node_iterator.h"

using vivaldi::NotesModelFactory;

namespace {

// Does a deep comparison of NoteNode fields in |model_a| and |model_b|.
// Returns true if they are all equal.
bool NodesMatch(const NoteNode* node_a, const NoteNode* node_b) {
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
    LOG(ERROR) << "Index mismatch: "
               << node_a->parent()->GetIndexOf(node_a).value() << " vs. "
               << node_b->parent()->GetIndexOf(node_b).value();
    return false;
  }
  return true;
}

// Checks if the hierarchies in |model_a| and |model_b| are equivalent in
// terms of the data model and favicon. Returns true if they both match.
// Note: Some peripheral fields like creation times are allowed to mismatch.
bool NotesModelsMatch(NotesModel* model_a, NotesModel* model_b) {
  bool ret_val = true;
  ui::TreeNodeIterator<const NoteNode> iterator_a(model_a->root_node());
  ui::TreeNodeIterator<const NoteNode> iterator_b(model_b->root_node());
  while (iterator_a.has_next()) {
    const NoteNode* node_a = iterator_a.Next();
    if (!iterator_b.has_next()) {
      LOG(ERROR) << "Models do not match.";
      return false;
    }
    const NoteNode* node_b = iterator_b.Next();
    ret_val = ret_val && NodesMatch(node_a, node_b);
  }
  ret_val = ret_val && (!iterator_b.has_next());
  return ret_val;
}

// Finds the node in the verifier notes model that corresponds to
// |foreign_node| in |foreign_model| and stores its address in |result|.
void FindNodeInVerifier(NotesModel* foreign_model,
                        const NoteNode* foreign_node,
                        const NoteNode** result) {
  // Climb the tree.
  std::stack<size_t> path;
  const NoteNode* walker = foreign_node;
  while (walker != foreign_model->root_node()) {
    path.push(walker->parent()->GetIndexOf(walker).value());
    walker = walker->parent();
  }

  // Swing over to the other tree.
  walker = notes_helper::GetVerifierNotesModel()->root_node();

  // Climb down.
  while (!path.empty()) {
    ASSERT_TRUE(walker->is_folder());
    ASSERT_LT(path.top(), walker->children().size());
    walker = walker->children()[path.top()].get();
    path.pop();
  }

  ASSERT_TRUE(NodesMatch(foreign_node, walker));
  *result = walker;
}

}  // namespace

namespace notes_helper {

NotesModel* GetNotesModel(int index) {
  return NotesModelFactory::GetForBrowserContext(
      sync_datatype_helper::test()->GetProfile(index));
}

const NoteNode* GetNotesTopNode(int index) {
  return GetNotesModel(index)->main_node();
}

NotesModel* GetVerifierNotesModel() {
  return NotesModelFactory::GetForBrowserContext(
      sync_datatype_helper::test()->verifier());
}

const NoteNode* AddNote(int profile,
                        const std::string& content,
                        const GURL& url) {
  return AddNote(profile, GetNotesTopNode(profile), 0, content, "", url);
}

const NoteNode* AddNote(int profile,
                        const std::string& content,
                        const std::string& title,
                        const GURL& url) {
  return AddNote(profile, GetNotesTopNode(profile), 0, content, title, url);
}

const NoteNode* AddNote(int profile,
                        int index,
                        const std::string& content,
                        const GURL& url) {
  return AddNote(profile, GetNotesTopNode(profile), index, content, "", url);
}

const NoteNode* AddNote(int profile,
                        int index,
                        const std::string& content,
                        const std::string& title,
                        const GURL& url) {
  return AddNote(profile, GetNotesTopNode(profile), index, content, title, url);
}

const NoteNode* AddNote(int profile,
                        const NoteNode* parent,
                        int index,
                        const std::string& content,
                        const GURL& url) {
  return AddNote(profile, parent, index, content, "", url);
}

const NoteNode* AddNote(int profile,
                        const NoteNode* parent,
                        int index,
                        const std::string& content,
                        const std::string& title,
                        const GURL& url) {
  NotesModel* model = GetNotesModel(profile);
  if (model->GetNoteNodeByID(parent->id()) != parent) {
    LOG(ERROR) << "Node " << parent->GetTitle() << " does not belong to "
               << "Profile " << profile;
    return NULL;
  }
  const NoteNode* result = model->AddNote(
      parent, index, base::UTF8ToUTF16(title), url, base::UTF8ToUTF16(content));
  if (!result) {
    LOG(ERROR) << "Could not add notes " << title << " to Profile " << profile;
    return NULL;
  }
  if (sync_datatype_helper::test()->UseVerifier()) {
    const NoteNode* v_parent = NULL;
    FindNodeInVerifier(model, parent, &v_parent);
    const NoteNode* v_node = GetVerifierNotesModel()->AddNote(
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

const NoteNode* AddFolder(int profile, const std::string& title) {
  return AddFolder(profile, GetNotesTopNode(profile), 0, title);
}

const NoteNode* AddFolder(int profile, int index, const std::string& title) {
  return AddFolder(profile, GetNotesTopNode(profile), index, title);
}

const NoteNode* AddFolder(int profile,
                          const NoteNode* parent,
                          int index,
                          const std::string& title) {
  NotesModel* model = GetNotesModel(profile);
  EXPECT_TRUE(model);
  EXPECT_TRUE(parent);
  if (model->GetNoteNodeByID(parent->id()) != parent) {
    LOG(ERROR) << "Node " << parent->GetTitle() << " does not belong to "
               << "Profile " << profile;
    return NULL;
  }
  const NoteNode* result =
      model->AddFolder(parent, index, base::UTF8ToUTF16(title));
  EXPECT_TRUE(result);
  if (!result) {
    LOG(ERROR) << "Could not add folder " << title << " to Profile " << profile;
    return NULL;
  }
  EXPECT_TRUE(result->parent() == parent);
  if (sync_datatype_helper::test()->UseVerifier()) {
    const NoteNode* v_parent = NULL;
    FindNodeInVerifier(model, parent, &v_parent);
    DCHECK(v_parent);
    const NoteNode* v_node = GetVerifierNotesModel()->AddFolder(
        v_parent, index, base::UTF8ToUTF16(title));
    if (!v_node) {
      LOG(ERROR) << "Could not add folder " << title << " to the verifier";
      return NULL;
    }
    EXPECT_TRUE(NodesMatch(v_node, result));
  }
  return result;
}

void SetTitle(int profile, const NoteNode* node, const std::string& new_title) {
  NotesModel* model = GetNotesModel(profile);
  ASSERT_EQ(model->GetNoteNodeByID(node->id()), node)
      << "Node " << node->GetTitle() << " does not belong to "
      << "Profile " << profile;
  if (sync_datatype_helper::test()->UseVerifier()) {
    const NoteNode* v_node = NULL;
    FindNodeInVerifier(model, node, &v_node);
    GetVerifierNotesModel()->SetTitle(v_node, base::UTF8ToUTF16(new_title));
  }
  model->SetTitle(node, base::UTF8ToUTF16(new_title));
}

void SetContent(int profile,
                const NoteNode* node,
                const std::string& new_content) {
  NotesModel* model = GetNotesModel(profile);
  ASSERT_EQ(model->GetNoteNodeByID(node->id()), node)
      << "Node " << node->GetTitle() << " does not belong to "
      << "Profile " << profile;
  if (sync_datatype_helper::test()->UseVerifier()) {
    const NoteNode* v_node = NULL;
    FindNodeInVerifier(model, node, &v_node);
    GetVerifierNotesModel()->SetContent(v_node, base::UTF8ToUTF16(new_content));
  }
  model->SetContent(node, base::UTF8ToUTF16(new_content));
}

const NoteNode* SetURL(int profile, const NoteNode* node, const GURL& new_url) {
  NotesModel* model = GetNotesModel(profile);
  if (model->GetNoteNodeByID(node->id()) != node) {
    LOG(ERROR) << "Node " << node->GetTitle() << " does not belong to "
               << "Profile " << profile;
    return NULL;
  }
  if (sync_datatype_helper::test()->UseVerifier()) {
    const NoteNode* v_node = NULL;
    FindNodeInVerifier(model, node, &v_node);
    if (v_node->is_note())
      GetVerifierNotesModel()->SetURL(v_node, new_url);
  }
  if (node->is_note())
    model->SetURL(node, new_url);
  return node;
}

void Move(int profile,
          const NoteNode* node,
          const NoteNode* new_parent,
          int index) {
  NotesModel* model = GetNotesModel(profile);
  ASSERT_EQ(model->GetNoteNodeByID(node->id()), node)
      << "Node " << node->GetTitle() << " does not belong to "
      << "Profile " << profile;
  if (sync_datatype_helper::test()->UseVerifier()) {
    const NoteNode* v_new_parent = NULL;
    const NoteNode* v_node = NULL;
    FindNodeInVerifier(model, new_parent, &v_new_parent);
    FindNodeInVerifier(model, node, &v_node);
    GetVerifierNotesModel()->Move(v_node, v_new_parent, index);
  }
  model->Move(node, new_parent, index);
}

void Remove(int profile, const NoteNode* parent, int index) {
  NotesModel* model = GetNotesModel(profile);
  ASSERT_EQ(model->GetNoteNodeByID(parent->id()), parent)
      << "Node " << parent->GetTitle() << " does not belong to "
      << "Profile " << profile;
  if (sync_datatype_helper::test()->UseVerifier()) {
    const NoteNode* v_parent = NULL;
    FindNodeInVerifier(model, parent, &v_parent);
    ASSERT_TRUE(NodesMatch(parent->children()[index].get(),
                           v_parent->children()[index].get()));
    GetVerifierNotesModel()->Remove(v_parent->children()[index].get(),
                                    FROM_HERE);
  }
  model->Remove(parent->children()[index].get(), FROM_HERE);
}

void RemoveAll(int profile) {
  if (sync_datatype_helper::test()->UseVerifier()) {
    const NoteNode* root_node = GetVerifierNotesModel()->root_node();
    for (auto& it_root : root_node->children()) {
      for (int j = it_root->children().size() - 1; j >= 0; --j) {
        GetVerifierNotesModel()->Remove(it_root->children()[j].get(),
                                        FROM_HERE);
      }
    }
  }
  GetNotesModel(profile)->RemoveAllUserNotes(FROM_HERE);
}

void SortChildren(int profile, const NoteNode* parent) {
  NotesModel* model = GetNotesModel(profile);
  ASSERT_EQ(model->GetNoteNodeByID(parent->id()), parent)
      << "Node " << parent->GetTitle() << " does not belong to "
      << "Profile " << profile;
  if (sync_datatype_helper::test()->UseVerifier()) {
    const NoteNode* v_parent = NULL;
    FindNodeInVerifier(model, parent, &v_parent);
    GetVerifierNotesModel()->SortChildren(v_parent);
  }
  model->SortChildren(parent);
}

void ReverseChildOrder(int profile, const NoteNode* parent) {
  ASSERT_EQ(GetNotesModel(profile)->GetNoteNodeByID(parent->id()), parent)
      << "Node " << parent->GetTitle() << " does not belong to "
      << "Profile " << profile;
  size_t child_count = parent->children().size();
  if (child_count == 0)
    return;
  for (size_t index = 0; index < child_count; ++index) {
    Move(profile, parent->children()[index].get(), parent, child_count - index);
  }
}

bool ModelMatchesVerifier(int profile) {
  if (!sync_datatype_helper::test()->UseVerifier()) {
    LOG(ERROR) << "Illegal to call ModelMatchesVerifier() after "
               << "DisableVerifier(). Use ModelsMatch() instead.";
    return false;
  }
  return NotesModelsMatch(GetVerifierNotesModel(), GetNotesModel(profile));
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

  bool IsExitConditionSatisfied(std::ostream* os) override;
};

AllModelsMatchChecker::AllModelsMatchChecker()
    : MultiClientStatusChangeChecker(
          sync_datatype_helper::test()->GetSyncServices()) {}

AllModelsMatchChecker::~AllModelsMatchChecker() {}

bool AllModelsMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  return AllModelsMatch();
}

}  //  namespace

bool AwaitAllModelsMatch() {
  AllModelsMatchChecker checker;
  checker.Wait();
  return !checker.TimedOut();
}

bool ContainsDuplicateNotes(int profile) {
  ui::TreeNodeIterator<const NoteNode> iterator(
      GetNotesModel(profile)->root_node());
  while (iterator.has_next()) {
    const NoteNode* node = iterator.Next();
    if (node->is_folder())
      continue;
    std::vector<const NoteNode*> nodes;
    GetNotesModel(profile)->GetNodesByURL(node->GetURL(), &nodes);
    EXPECT_GE(nodes.size(), 1ul);
    for (std::vector<const NoteNode*>::const_iterator it = nodes.begin();
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
  std::vector<const NoteNode*> nodes;
  GetNotesModel(profile)->GetNodesByURL(url, &nodes);
  return !nodes.empty();
}

const NoteNode* GetUniqueNodeByURL(int profile, const GURL& url) {
  std::vector<const NoteNode*> nodes;
  GetNotesModel(profile)->GetNodesByURL(url, &nodes);
  EXPECT_EQ(1U, nodes.size());
  if (nodes.empty())
    return NULL;
  return nodes[0];
}

int CountNotesWithContentMatching(int profile, const std::string& content) {
  auto utf16_content = base::UTF8ToUTF16(content);
  ui::TreeNodeIterator<const NoteNode> iterator(
      GetNotesModel(profile)->root_node());
  // Walk through the model tree looking for notes nodes of node type
  // note whose content match |content|.
  int count = 0;
  while (iterator.has_next()) {
    const NoteNode* node = iterator.Next();
    if ((node->type() == NoteNode::NOTE) &&
        (node->GetContent() == utf16_content))
      ++count;
  }
  return count;
}

int CountFoldersWithTitlesMatching(int profile, const std::string& title) {
  auto utf16_title = base::UTF8ToUTF16(title);
  ui::TreeNodeIterator<const NoteNode> iterator(
      GetNotesModel(profile)->root_node());
  // Walk through the model tree looking for notes nodes of node type
  // folder whose titles match |title|.
  int count = 0;
  while (iterator.has_next()) {
    const NoteNode* node = iterator.Next();
    if ((node->type() == NoteNode::FOLDER) && (node->GetTitle() == utf16_title))
      ++count;
  }
  return count;
}

}  // namespace notes_helper
