// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/notes/notes_api.h"

#include "../../notes/notesnode.h"
#include "../../notes/notes_factory.h"
#include "../../notes/notes_model.h"
#include "base/strings/utf_string_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/i18n/string_search.h"
#include "extensions/browser/event_router.h"
#include "ui/base/models/tree_node_iterator.h"
#include "base/strings/string_number_conversions.h"

namespace extensions {

const char noteNotFoundStr[] = "Note not found.";

///// TODO MOVE TO SEPARATE FILE

void BroadcastEvent(const std::string& eventname,
                    scoped_ptr<base::ListValue> args,
                    content::BrowserContext* context) {
  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::UNKNOWN, eventname, args.Pass()));
  event->restrict_to_browser_context = context;
  EventRouter* event_router = EventRouter::Get(context);
  if (event_router) {
    event_router->BroadcastEvent(event.Pass());
  }
}

NoteAttachment* CreateNoteAttachment(Notes_attachment* attachment) {
  NoteAttachment* note_attachment = new NoteAttachment();

  base::string16 fname;
  base::string16 cnt_type;
  std::string* content = new std::string();
  content->assign(attachment->content);
  note_attachment->content.reset(content);
  note_attachment->filename.reset(new std::string(base::UTF16ToUTF8(fname)));
  note_attachment->content_type.reset(
      new std::string(base::UTF16ToUTF8(cnt_type)));
  return note_attachment;
}

NoteTreeNode* CreateTreeNode(Notes_Node* node) {
  NoteTreeNode* notes_tree_node = new NoteTreeNode;

  notes_tree_node->id = base::Int64ToString(node->id());

  const Notes_Node* parent = node->parent();
  if (parent) {
    notes_tree_node->parent_id.reset(
        new std::string(base::Int64ToString(parent->id())));
    notes_tree_node->index.reset(new int(parent->GetIndexOf(node)));
  }

  notes_tree_node->title.reset(
      new std::string(base::UTF16ToUTF8(node->GetTitle())));
  notes_tree_node->content.reset(
      new std::string(base::UTF16ToUTF8(node->GetContent())));

  if (node->GetURL().is_valid()) {
    notes_tree_node->url.reset(new std::string(node->GetURL().spec()));
  }
  std::vector<linked_ptr<NoteAttachment>> newattachments;

  std::vector<Notes_attachment> attachments = node->GetAttachments();
  for (unsigned int i = 0; i < attachments.size(); i++) {
    linked_ptr<NoteAttachment> attachment(
        CreateNoteAttachment(&(attachments[i])));
    newattachments.push_back(attachment);
  }
  notes_tree_node->attachments.reset(
      new std::vector<linked_ptr<NoteAttachment>>(newattachments));

  // Javascript Date wants milliseconds since the epoch, ToDoubleT is seconds.
  double timedouble = node->GetCreationTime().ToDoubleT();
  notes_tree_node->date_added.reset(new double(floor(timedouble * 1000)));

  if (node->is_folder()) {
    std::vector<linked_ptr<NoteTreeNode>> children;
    for (int i = 0; i < node->child_count(); ++i) {
      Notes_Node* child = node->GetChild(i);
      linked_ptr<NoteTreeNode> child_node(CreateTreeNode(child));
      children.push_back(child_node);
    }
    notes_tree_node->children.reset(
        new std::vector<linked_ptr<NoteTreeNode>>(children));
  }
  return notes_tree_node;
}

///// TODO MOVE TO SEPARATE FILE  ^^^

NotesGetFunction::NotesGetFunction() {}

Notes_Node* NotesAsyncFunction::GetNodeFromId(Notes_Node* node, int64 id) {
  if (node->id() == id) {
    return node;
  }
  int number_of_notes = node->child_count();
  for (int i = 0; i < number_of_notes; i++) {
    Notes_Node* childnode = GetNodeFromId(node->GetChild(i), id);
    if (childnode) {
      return childnode;
    }
  }
  return NULL;
}

bool NotesGetFunction::RunAsync() {
  scoped_ptr<api::notes::Get::Params> params(
      api::notes::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::vector<linked_ptr<NoteTreeNode>> notes;
  Notes_Model* model = Vivaldi::NotesModelFactory::GetForProfile(GetProfile());
  Notes_Node* root = model->root();
  if (params->id_or_id_list.as_strings) {
    std::vector<std::string>& ids = *params->id_or_id_list.as_strings;
    size_t count = ids.size();
    EXTENSION_FUNCTION_VALIDATE(count > 0);
    for (size_t i = 0; i < count; ++i) {
      int64 idval;
      base::StringToInt64(ids[i], &idval);
      Notes_Node* node = GetNodeFromId(root, idval);
      if (!node) {
        error_ = noteNotFoundStr;
        SendResponse(false);
        return false;
      }
      linked_ptr<NoteTreeNode> note(CreateTreeNode(node));
      notes.push_back(note);
    }
  } else {
    int64 idval;
    base::StringToInt64(*params->id_or_id_list.as_string, &idval);
    Notes_Node* node = GetNodeFromId(root, idval);
    if (!node) {
      error_ = noteNotFoundStr;
      SendResponse(false);
      return false;
    }
    linked_ptr<NoteTreeNode> note(CreateTreeNode(node));
    notes.push_back(note);
  }

  results_ = api::notes::Get::Results::Create(notes);

  SendResponse(true);
  return true;
}

NotesGetFunction::~NotesGetFunction() {}

NotesGetTreeFunction::NotesGetTreeFunction() {}

bool NotesGetTreeFunction::RunAsync() {
  std::vector<linked_ptr<NoteTreeNode>> notes;
  Notes_Model* model = Vivaldi::NotesModelFactory::GetForProfile(GetProfile());
  Notes_Node* root = model->root();
  linked_ptr<NoteTreeNode> new_note(CreateTreeNode(root));
  if (new_note->children.get()->size())  // Do not return root.
    notes.push_back(new_note);
  results_ = api::notes::GetTree::Results::Create(notes);
  SendResponse(true);
  return true;
}

NotesGetTreeFunction::~NotesGetTreeFunction() {}

NotesCreateFunction::NotesCreateFunction() {}

bool NotesCreateFunction::RunAsync() {
  scoped_ptr<api::notes::Create::Params> params(
      api::notes::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Notes_Model* model = Vivaldi::NotesModelFactory::GetForProfile(GetProfile());

  Notes_Node* parent = NULL;

  int64 newIndex = model->getNewIndex();
  Notes_Node* newnode = new Notes_Node(newIndex);
  // Lots of optionals, make sure to check for the precence.
  if (params->note.title.get()) {
    base::string16 title;
    title = base::UTF8ToUTF16(*params->note.title);
    newnode->SetTitle(title);
  }

  if (params->note.type.get()) {
    std::string type = (*params->note.type);
    newnode->SetType((type == "note" ? Notes_Node::NOTE : Notes_Node::FOLDER));
  } else {
    // default to note
    newnode->SetType(Notes_Node::NOTE);
  }

  if (params->note.content.get()) {
    base::string16 content;
    content = base::UTF8ToUTF16(*params->note.content);
    newnode->SetContent(content);
  }

  if (params->note.url.get()) {
    GURL url(*params->note.url);
    newnode->SetURL(url);
  }

  if (params->note.parent_id.get()) {
    int64 idval;
    base::StringToInt64(*params->note.parent_id.get(), &idval);
    parent = GetNodeFromId(model->root(), idval);
  }

  // insert the attachments
  if (params->note.attachments) {
    for (unsigned int i = 0; i < params->note.attachments->size(); i++) {
      Notes_attachment* attachment = new Notes_attachment();
      attachment->content = *params->note.attachments->at(i)->content.get();
      if (params->note.attachments->at(i)->content_type.get())
        attachment->content_type = base::UTF8ToUTF16(
            *params->note.attachments->at(i)->content_type.get());
      if (params->note.attachments->at(i)->filename.get())
        attachment->filename =
            base::UTF8ToUTF16(*params->note.attachments->at(i)->filename.get());
      newnode->AddAttachment(*attachment);
    }
  }

  if (!parent) {
    parent = model->root();
  }

  model->AddNode(parent, parent->child_count(), newnode);

  scoped_ptr<api::notes::NoteTreeNode> treenode(CreateTreeNode(newnode));

  results_ = api::notes::Create::Results::Create(*treenode.get());

  SendResponse(true);

  scoped_ptr<base::ListValue> args = api::notes::OnCreated::Create(
      base::Int64ToString(newnode->id()), *treenode.Pass());

  BroadcastEvent("notes.onCreated", args.Pass(), context_);
  return true;
}

NotesCreateFunction::~NotesCreateFunction() {}

NotesUpdateFunction::NotesUpdateFunction() {}

bool NotesUpdateFunction::RunAsync() {
  scoped_ptr<api::notes::Update::Params> params(
      api::notes::Update::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Notes_Model* model = Vivaldi::NotesModelFactory::GetForProfile(GetProfile());
  int64 idval;
  base::StringToInt64(params->id, &idval);
  Notes_Node* node = GetNodeFromId(model->root(), idval);
  if (!node) {
    error_ = noteNotFoundStr;
    SendResponse(false);
    DCHECK(node);
    return false;
  }

  api::notes::OnChanged::ChangeInfo changeinfo;

  // All fields are optional.
  base::string16 title;
  if (params->changes.title.get()) {
    title = base::UTF8ToUTF16(*params->changes.title);
    node->SetTitle(title);
    changeinfo.title = *params->changes.title;
  }

  std::string content;
  if (params->changes.content.get()) {
    content = (*params->changes.content);
    node->SetContent(base::UTF8ToUTF16(content));
    changeinfo.content = content;
  }

  double dateGroupModified;
  if (params->changes.date_group_modified.get()) {
    dateGroupModified = (*params->changes.date_group_modified);
  }

  double dateAdded;
  if (params->changes.date_added.get()) {
    dateAdded = (*params->changes.date_added);
  }

  std::string url_string;
  if (params->changes.url.get()) {
    url_string = *params->changes.url;
    GURL url(url_string);
    node->SetURL(url);
    changeinfo.url.reset(new std::string(*params->changes.url));
  }

  if (params->changes.attachments.get()) {
    // Delete all the current attachments if changed.
    while (node->GetAttachments().size()) {
      node->DeleteAttachment(0);
    }
    for (unsigned int i = 0; i < params->changes.attachments->size(); i++) {
      Notes_attachment* attachment = new Notes_attachment();
      attachment->content = *params->changes.attachments->at(i)->content.get();
      if (params->changes.attachments->at(i)->content_type)
        attachment->content_type = base::UTF8ToUTF16(
            *params->changes.attachments->at(i)->content_type.get());
      if (params->changes.attachments->at(i)->content)
        attachment->content =
            *(params->changes.attachments->at(i)->content.get());
      if (params->changes.attachments->at(i)->filename)
        attachment->filename = base::UTF8ToUTF16(
            *params->changes.attachments->at(i)->filename.get());
      node->AddAttachment(*attachment);
    }
  }

  scoped_ptr<api::notes::NoteTreeNode> ret(CreateTreeNode(node));

  results_ = api::notes::Create::Results::Create(*ret);

  SendResponse(true);

  scoped_ptr<base::ListValue> args = api::notes::OnChanged::Create(
      base::Int64ToString(node->id()), changeinfo);

  BroadcastEvent("notes.onChanged", args.Pass(), context_);

  return model->SaveNotes();
}

NotesUpdateFunction::~NotesUpdateFunction() {}

NotesRemoveFunction::NotesRemoveFunction() {}

bool NotesRemoveFunction::RunAsync() {
  scoped_ptr<api::notes::Remove::Params> params(
      api::notes::Remove::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int64 id;
  base::StringToInt64(params->id, &id);

  Notes_Model* model = Vivaldi::NotesModelFactory::GetForProfile(GetProfile());

  Notes_Node* root = model->root();
  Notes_Node* node = GetNodeFromId(root, id);
  if (!node) {
    error_ = noteNotFoundStr;
    SendResponse(false);
    DCHECK(node);
    return false;
  }
  Notes_Node* parent = node->parent();
  if (!parent) {
    parent = root;
  }

  int indexofdeleted = parent->GetIndexOf(node);
  if (!model->Remove(parent, indexofdeleted)) {
    error_ = noteNotFoundStr;
    SendResponse(false);
    return false;
  }

  SendResponse(true);

  api::notes::OnRemoved::RemoveInfo info;

  info.parent_id = parent->id();
  info.index = indexofdeleted;

  scoped_ptr<base::ListValue> args =
      api::notes::OnRemoved::Create(base::Int64ToString(id), info);

  BroadcastEvent("notes.onRemoved", args.Pass(), context_);

  return true;
}

NotesRemoveFunction::~NotesRemoveFunction() {}

NotesRemoveTreeFunction::NotesRemoveTreeFunction() {}

bool NotesRemoveTreeFunction::RunAsync() {
  return true;
}

NotesRemoveTreeFunction::~NotesRemoveTreeFunction() {}

NotesSearchFunction::NotesSearchFunction() {}

bool NotesSearchFunction::RunAsync() {
  scoped_ptr<api::notes::Search::Params> params(
      api::notes::Search::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::string16 searchstring = base::UTF8ToUTF16(params->query);

  std::vector<linked_ptr<api::notes::NoteTreeNode>> search_result;

  // loop through the tree and match on content
  Notes_Model* model = Vivaldi::NotesModelFactory::GetForProfile(GetProfile());
  Notes_Node* root = model->root();

  ui::TreeNodeIterator<Notes_Node> iterator(root);
  while (iterator.has_next()) {
    Notes_Node* node = iterator.Next();

    // I found this strange for the iterator to iterate on children in folders.
    // These are added as children in the folder. See CreateTreeNode
    // Note that the tree is flattened when searchstring has content.
    bool show_at_rootlevel =
        node->parent()->is_root() || (searchstring.length() > 0);
    if (show_at_rootlevel &&
        base::i18n::StringSearchIgnoringCaseAndAccents(
            searchstring, node->GetContent(), NULL, NULL)) {
      linked_ptr<api::notes::NoteTreeNode> note(CreateTreeNode(node));
      search_result.push_back(note);
    }
  }

  results_ = api::notes::Search::Results::Create(search_result);
  SendResponse(true);
  return true;
}

NotesSearchFunction::~NotesSearchFunction() {}

NotesMoveFunction::NotesMoveFunction() {}

bool NotesMoveFunction::RunAsync() {
  scoped_ptr<api::notes::Move::Params> params(
    api::notes::Move::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Notes_Model* model = Vivaldi::NotesModelFactory::GetForProfile(GetProfile());
  Notes_Node* root = model->root();

  int64 id;
  base::StringToInt64(params->id, &id);
  if (!id)
    return false;
  Notes_Node* node = GetNodeFromId(root, id);
  if (!node)
    return false;
  Notes_Node* old_parent = node->parent();
  int oldIndex = old_parent->GetIndexOf(node);
  std::string aid = base::Int64ToString(node->parent()->id());

  const Notes_Node* parent = NULL;
  if (!params->destination.parent_id.get()) {
    // Optional, defaults to current parent.
    parent = node->parent();
  } else {
    int64 parentId;
    if (!base::StringToInt64(*params->destination.parent_id, &parentId))
      return false;
    parent = GetNodeFromId(root, parentId);
  }

  int index;

  if (params->destination.index.get()) {  // Optional (defaults to end).
    index = *params->destination.index;
    if (index > parent->child_count() || index < 0) {
      error_ = "Index out of bounds.";  // Todo move to constant
      return false;
    }
  } else {
    index = parent->child_count();
  }
  model->Move(node, parent, index);

  scoped_ptr<api::notes::NoteTreeNode> ret(CreateTreeNode(node));
  results_ = api::notes::Move::Results::Create(*ret);

  api::notes::OnMoved::MoveInfo moveInfo;

  moveInfo.index = index;
  moveInfo.old_index = oldIndex;

  moveInfo.parent_id = base::Int64ToString(parent->id());
  moveInfo.old_parent_id = aid;

  scoped_ptr<base::ListValue> args =
      api::notes::OnMoved::Create(base::Int64ToString(node->id()), moveInfo);

  BroadcastEvent("notes.onMoved", args.Pass(), context_);

  SendResponse(true);
  return true;
}

NotesMoveFunction::~NotesMoveFunction() {}

}  // namespace extensions
