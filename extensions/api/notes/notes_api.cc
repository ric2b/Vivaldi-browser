// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/notes/notes_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/i18n/string_search.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/schema/notes.h"
#include "notes/notes_factory.h"
#include "notes/notes_model.h"
#include "notes/notesnode.h"
#include "ui/base/models/tree_node_iterator.h"

using vivaldi::NotesModelFactory;
using vivaldi::Notes_Model;
using vivaldi::Notes_Node;
using extensions::vivaldi::notes::NoteTreeNode;
using extensions::vivaldi::notes::NoteAttachment;

namespace extensions {

static base::LazyInstance<BrowserContextKeyedAPIFactory<NotesAPI> >::
    DestructorAtExit g_factory_notes = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<NotesAPI>* NotesAPI::GetFactoryInstance() {
  return g_factory_notes.Pointer();
}

NotesAPI::NotesAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this,
                                 vivaldi::notes::OnImportBegan::kEventName);
  event_router->RegisterObserver(this,
                                 vivaldi::notes::OnImportEnded::kEventName);
}

NotesAPI::~NotesAPI() {
  DCHECK(!model_);
}

void NotesAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
  if (model_) {
    model_->RemoveObserver(this);
    model_ = nullptr;
  }
}

void NotesAPI::OnListenerAdded(const EventListenerInfo& details) {
  DCHECK(!model_);
  model_ = NotesModelFactory::GetForBrowserContext(browser_context_);
  model_->AddObserver(this);
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

void NotesAPI::ExtensiveNotesChangesBeginning(Notes_Model* model) {
  DCHECK(model_ == model);
  ::vivaldi::BroadcastEvent(vivaldi::notes::OnImportBegan::kEventName,
                            vivaldi::notes::OnImportBegan::Create(),
                            browser_context_);
}

void NotesAPI::ExtensiveNotesChangesEnded(Notes_Model* model) {
  DCHECK(model_ == model);
  ::vivaldi::BroadcastEvent(vivaldi::notes::OnImportEnded::kEventName,
                            vivaldi::notes::OnImportEnded::Create(),
                            browser_context_);
}

namespace {

void SendCreated(BrowserContext* browser_context,
                 int64_t id,
                 const NoteTreeNode& treenode) {
  ::vivaldi::BroadcastEvent(
      vivaldi::notes::OnCreated::kEventName,
      vivaldi::notes::OnCreated::Create(base::Int64ToString(id), treenode),
      browser_context);
}

void SendChanged(BrowserContext* browser_context,
                 int64_t id,
                 const vivaldi::notes::OnChanged::ChangeInfo& change_info) {
  ::vivaldi::BroadcastEvent(
      vivaldi::notes::OnChanged::kEventName,
      vivaldi::notes::OnChanged::Create(base::Int64ToString(id), change_info),
      browser_context);
}

void SendMoved(BrowserContext* browser_context,
               int64_t id,
               int64_t parent_id,
               int index,
               int64_t old_parent_id,
               int old_index) {
  vivaldi::notes::OnMoved::MoveInfo move_info;

  move_info.index = index;
  move_info.old_index = old_index;
  move_info.parent_id = base::Int64ToString(parent_id);
  move_info.old_parent_id = base::Int64ToString(old_parent_id);

  ::vivaldi::BroadcastEvent(
      vivaldi::notes::OnMoved::kEventName,
      vivaldi::notes::OnMoved::Create(base::Int64ToString(id), move_info),
      browser_context);
}

void SendRemoved(BrowserContext* browser_context,
                 int64_t id,
                 int64_t parent_id,
                 int indexofdeleted) {
  vivaldi::notes::OnRemoved::RemoveInfo info;

  info.parent_id = base::Int64ToString(parent_id);
  info.index = indexofdeleted;

  ::vivaldi::BroadcastEvent(
      vivaldi::notes::OnRemoved::kEventName,
      vivaldi::notes::OnRemoved::Create(base::Int64ToString(id), info),
      browser_context);
}

NoteAttachment MakeNoteAttachment(const ::vivaldi::NoteAttachment& attachment) {
  NoteAttachment note_attachment;
  note_attachment.content = attachment.content();
  note_attachment.checksum =
      std::make_unique<std::string>(attachment.checksum());
  return note_attachment;
}

std::unique_ptr<std::vector<NoteAttachment>> CreateNoteAttachments(
    const ::vivaldi::NoteAttachments& attachments) {
  auto new_attachments = std::make_unique<std::vector<NoteAttachment>>();
  for (const auto& attachment : attachments) {
    new_attachments->push_back(MakeNoteAttachment(attachment.second));
  }

  return new_attachments;
}

NoteTreeNode MakeTreeNode(Notes_Node* node) {
  NoteTreeNode notes_tree_node;

  notes_tree_node.id = base::Int64ToString(node->id());

  const Notes_Node* parent = node->parent();
  if (parent) {
    notes_tree_node.parent_id.reset(
        new std::string(base::Int64ToString(parent->id())));
    notes_tree_node.index.reset(new int(parent->GetIndexOf(node)));
  }
  notes_tree_node.trash.reset(new bool(node->is_trash()));

  notes_tree_node.separator.reset(new bool(node->is_separator()));

  notes_tree_node.title.reset(
      new std::string(base::UTF16ToUTF8(node->GetTitle())));
  notes_tree_node.content.reset(
      new std::string(base::UTF16ToUTF8(node->GetContent())));

  if (node->GetURL().is_valid()) {
    notes_tree_node.url.reset(new std::string(node->GetURL().spec()));
  }

  notes_tree_node.attachments = CreateNoteAttachments(node->GetAttachments());

  // Javascript Date wants milliseconds since the epoch, ToDoubleT is seconds.
  double timedouble = node->GetCreationTime().ToDoubleT();
  notes_tree_node.date_added.reset(new double(floor(timedouble * 1000)));

  if (node->is_folder()) {
    std::vector<NoteTreeNode> children;
    for (int i = 0; i < node->child_count(); ++i) {
      Notes_Node* child = node->GetChild(i);
      children.push_back(MakeTreeNode(child));
    }
    notes_tree_node.children.reset(
        new std::vector<NoteTreeNode>(std::move(children)));
  }
  return notes_tree_node;
}

Notes_Model* GetNotesModel(UIThreadExtensionFunction* fun) {
  return NotesModelFactory::GetForBrowserContext(fun->browser_context());
}

Notes_Node* GetNodeFromId(Notes_Node* node, int64_t id) {
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
  return nullptr;
}

Notes_Node* ParseNoteId(Notes_Model* model,
                        const std::string& id_str,
                        std::string* error) {
  int64_t id;
  if (!base::StringToInt64(id_str, &id)) {
    *error = "Note id is not a number - " + id_str;
    return nullptr;
  }
  Notes_Node* node = GetNodeFromId(model->root_node(), id);
  if (!node) {
    *error = "No note with id " + id_str;
    return nullptr;
  }
  return node;
}

}  // namespace

///// TODO MOVE TO SEPARATE FILE  ^^^


ExtensionFunction::ResponseAction NotesGetFunction::Run() {
  std::unique_ptr<vivaldi::notes::Get::Params> params(
      vivaldi::notes::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::vector<NoteTreeNode> notes;
  Notes_Model* model = GetNotesModel(this);
  std::string error;
  if (params->id_or_id_list.as_strings) {
    std::vector<std::string>& ids = *params->id_or_id_list.as_strings;
    size_t count = ids.size();
    EXTENSION_FUNCTION_VALIDATE(count > 0);
    for (size_t i = 0; i < count; ++i) {
      Notes_Node* node = ParseNoteId(model, ids[i], &error);
      if (!node) {
        return RespondNow(Error(error));
      }
      notes.push_back(MakeTreeNode(node));
    }
  } else {
    Notes_Node* node =
        ParseNoteId(model, *params->id_or_id_list.as_string, &error);
    if (!node) {
      return RespondNow(Error(error));
    }
    notes.push_back(MakeTreeNode(node));
  }
  return RespondNow(ArgumentList(vivaldi::notes::Get::Results::Create(notes)));
}

ExtensionFunction::ResponseAction NotesGetTreeFunction::Run() {
  Notes_Model* model = GetNotesModel(this);

  // If the model has not loaded yet wait until it does and do the work then.
  if (!model->loaded()) {
    AddRef();  // Balanced in NotesModelLoaded and NotesModelBeingDeleted.
    model->AddObserver(this);
    return RespondLater();
  } else {
    SendGetTreeResponse(model);
    return AlreadyResponded();
  }
}

void NotesGetTreeFunction::SendGetTreeResponse(Notes_Model* model) {
  std::vector<NoteTreeNode> notes;
  Notes_Node* root = model->main_node();
  NoteTreeNode new_note = MakeTreeNode(root);
  new_note.children->push_back(MakeTreeNode(model->trash_node()));
  // After the above push the condition is always true
  //if (new_note.children->size()) {  // Do not return root.
    notes.push_back(std::move(new_note));
  //}
  Respond(ArgumentList(vivaldi::notes::GetTree::Results::Create(notes)));
}

void NotesGetTreeFunction::NotesModelLoaded(Notes_Model* model,
                                            bool ids_reassigned) {
  SendGetTreeResponse(model);
  model->RemoveObserver(this);
  Release();
}

void NotesGetTreeFunction::NotesModelBeingDeleted(Notes_Model* model) {
  Respond(Error("NotesModelBeingDeleted"));
  model->RemoveObserver(this);
  Release();
}

ExtensionFunction::ResponseAction NotesCreateFunction::Run() {
  std::unique_ptr<vivaldi::notes::Create::Params> params(
      vivaldi::notes::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Notes_Model* model = GetNotesModel(this);

  int64_t id = model->GetNewIndex();
  std::unique_ptr<Notes_Node> newnode = std::make_unique<Notes_Node>(id);
  Notes_Node* newnode_ptr = newnode.get();
  // Lots of optionals, make sure to check for the contents.
  if (params->note.title.get()) {
    base::string16 title;
    title = base::UTF8ToUTF16(*params->note.title);
    newnode->SetTitle(title);
  }

  if (params->note.type.get()) {
    std::string type = (*params->note.type);
    if (type == "note")
      newnode->SetType(Notes_Node::NOTE);
    else if (type == "separator")
      newnode->SetType(Notes_Node::SEPARATOR);
    else
      newnode->SetType(Notes_Node::FOLDER);
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

  Notes_Node* parent = nullptr;
  if (params->note.parent_id.get()) {
    std::string error;
    parent = ParseNoteId(model, *params->note.parent_id, &error);
    if (!parent) {
      return RespondNow(Error(error));
    }
  }

  // insert the attachments
  if (params->note.attachments) {
    for (const auto& attachment : *(params->note.attachments)) {
      newnode->AddAttachment(::vivaldi::NoteAttachment(attachment.content));
    }
  }

  if (!parent || parent == model->root_node()) {
    parent = model->main_node();
  }
  if (parent == model->main_node()) {
    int64_t maxIndex = parent->child_count();
    int64_t newIndex = maxIndex;
    if (params->note.index.get()) {
      newIndex = *params->note.index.get();
      if (newIndex > maxIndex) {
        newIndex = maxIndex;
      }
    }
    model->AddNode(parent, newIndex, std::move(newnode));
  } else {
    int64_t newIndex = parent->child_count();
    if (params->note.index.get()) {
      newIndex = *params->note.index.get();
    }
    model->AddNode(parent, newIndex, std::move(newnode));
  }

  vivaldi::notes::NoteTreeNode treenode = MakeTreeNode(newnode_ptr);

  SendCreated(browser_context(), newnode_ptr->id(), treenode);

  return RespondNow(
      ArgumentList(vivaldi::notes::Create::Results::Create(treenode)));
}

ExtensionFunction::ResponseAction NotesUpdateFunction::Run() {
  std::unique_ptr<vivaldi::notes::Update::Params> params(
      vivaldi::notes::Update::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Notes_Model* model = GetNotesModel(this);
  std::string error;
  Notes_Node* node = ParseNoteId(model, params->id, &error);
  if (!node) {
    return RespondNow(Error(error));
  }

  vivaldi::notes::OnChanged::ChangeInfo changeinfo;

  // All fields are optional.
  base::string16 title;
  if (params->changes.title.get()) {
    title = base::UTF8ToUTF16(*params->changes.title);
    model->SetTitle(node, title);
    changeinfo.title.reset(new std::string(*params->changes.title));
  }

  std::string content;
  if (params->changes.content.get()) {
    content = (*params->changes.content);
    model->SetContent(node, base::UTF8ToUTF16(content));
    changeinfo.content.reset(new std::string(*params->changes.content));
  }

  std::string url_string;
  if (params->changes.url.get()) {
    url_string = *params->changes.url;
    GURL url(url_string);
    model->SetURL(node, url);
    changeinfo.url.reset(new std::string(*params->changes.url));
  }

  if (params->changes.attachments.get()) {
    model->ClearAttachments(node);
    for (const auto& attachment : *(params->changes.attachments)) {
      if (attachment.checksum)
        model->AddAttachment(node,
                             ::vivaldi::NoteAttachment(*(attachment.checksum),
                                                       attachment.content));
      else
        model->AddAttachment(node,
                             ::vivaldi::NoteAttachment(attachment.content));
    }

    changeinfo.attachments = CreateNoteAttachments(node->GetAttachments());
  }

  if (!model->SaveNotes()) {
    return RespondNow(Error("SaveNotes failed"));
  }

  // Respond before we send the event.
  Respond(ArgumentList(
      vivaldi::notes::Create::Results::Create(MakeTreeNode(node))));
  SendChanged(browser_context(), node->id(), changeinfo);
  return AlreadyResponded();
}

ExtensionFunction::ResponseAction NotesRemoveFunction::Run() {
  std::unique_ptr<vivaldi::notes::Remove::Params> params(
      vivaldi::notes::Remove::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Notes_Model* model = GetNotesModel(this);
  std::string error;
  Notes_Node* node = ParseNoteId(model, params->id, &error);
  if (!node) {
    return RespondNow(Error(error));
  }
  Notes_Node* parent = node->parent();
  if (!parent) {
    parent = model->root_node();
  }
  Notes_Node* trash_node = model->trash_node();
  if (trash_node == node) {
    // Trying to delete trash, should not happen.
    NOTREACHED();
    return RespondNow(Error("Attempt to delete trash"));
  }

  bool move_to_trash = true;
  if (node->is_separator()) {
    move_to_trash = false;
  } else {
    Notes_Node* tmp = node;
    while (!tmp->is_root()) {
      if (tmp->parent() == trash_node) {
        move_to_trash = false;
        break;
      }
      tmp = tmp->parent();
    }
  }

  int indexofdeleted = parent->GetIndexOf(node);
  // TODO(pettern): Event notifications should be handled in the observer
  // for the model.
  if (move_to_trash) {
    Notes_Node* old_parent = node->parent();
    int oldIndex = old_parent->GetIndexOf(node);

    // Move to trash
    if (!model->Move(node, trash_node, 0)) {
      return RespondNow(Error("Failed to move note to trash - " + params->id));
    }

    // Respond before sending the event.
    Respond(ArgumentList(vivaldi::notes::Remove::Results::Create()));
    SendMoved(browser_context(), node->id(), trash_node->id(), 0,
              old_parent->id(), oldIndex);
    return AlreadyResponded();
  } else {
    int64_t idofdeleted = node->id();
    if (!model->Remove(parent, indexofdeleted)) {
      return RespondNow(Error("Failed to remove note " + params->id));
    }

    // Respond before sending the event.
    Respond(ArgumentList(vivaldi::notes::Remove::Results::Create()));
    SendRemoved(browser_context(), idofdeleted, parent->id(), indexofdeleted);
    return AlreadyResponded();
  }
}

ExtensionFunction::ResponseAction NotesSearchFunction::Run() {
  std::unique_ptr<vivaldi::notes::Search::Params> params(
      vivaldi::notes::Search::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::vector<vivaldi::notes::NoteTreeNode> search_result;

  bool examine_url = true;
  bool examine_content = true;
  size_t offset = 0;
  if (params->query.find("URL:") == 0) {
    examine_content = false;
    offset = 4;
  } else if (params->query.find("CONTENT:") == 0) {
    offset = 8;
    examine_url = false;
  }
  if (params->query.compare(offset, std::string::npos, ".") == 0) {
    examine_url = false;
  }

  base::string16 needle = base::UTF8ToUTF16(params->query.substr(offset));
  if (needle.length() > 0) {
    ui::TreeNodeIterator<Notes_Node> iterator(GetNotesModel(this)->root_node());

    while (iterator.has_next()) {
      Notes_Node* node = iterator.Next();
      bool match = false;
      if (examine_content) {
        match = base::i18n::StringSearchIgnoringCaseAndAccents(
            needle, node->GetContent(), NULL, NULL);
      }
      if (!match && examine_url && node->GetURL().is_valid()) {
        std::string value = node->GetURL().host() + node->GetURL().path();
        match = base::i18n::StringSearchIgnoringCaseAndAccents(
            needle, base::UTF8ToUTF16(value), NULL, NULL);
      }
      if (match) {
        search_result.push_back(MakeTreeNode(node));
      }
    }
  }

  return RespondNow(
      ArgumentList(vivaldi::notes::Search::Results::Create(search_result)));
}

ExtensionFunction::ResponseAction NotesMoveFunction::Run() {
  std::unique_ptr<vivaldi::notes::Move::Params> params(
      vivaldi::notes::Move::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Notes_Model* model = GetNotesModel(this);

  std::string error;
  Notes_Node* node = ParseNoteId(model, params->id, &error);
  if (!node) {
    return RespondNow(Error(error));
  }
  const Notes_Node* parent;
  if (!params->destination.parent_id.get()) {
    // Optional, defaults to current parent.
    parent = node->parent();
  } else {
    parent = ParseNoteId(model, *params->destination.parent_id, &error);
    if (!parent) {
      return RespondNow(Error(error));
    }
  }

  int index;
  if (params->destination.index.get()) {  // Optional (defaults to end).
    index = *params->destination.index;
    if (index > parent->child_count() || index < 0) {
       // Todo move to constant
      return RespondNow(Error("Index out of bounds."));
    }
  } else {
    index = parent->child_count();
  }

  Notes_Node* old_parent = node->parent();
  int64_t old_parent_id = old_parent->id();
  int old_index = old_parent->GetIndexOf(node);

  bool moved = model->Move(node, parent, index);
  DCHECK(moved);
  if (!moved) {
    // This will happen if we attempt to move a folder into a subfolder of its
    // own. Should never happen (missing test in JS), but better be on the safe
    // side as a reply will mess up the displayed data.
    return RespondNow(Error("Cannot move node"));
  }

  SendMoved(browser_context(), node->id(), parent->id(), index, old_parent_id,
            old_index);

  return RespondNow(ArgumentList(
      vivaldi::notes::Move::Results::Create(MakeTreeNode(node))));
}

ExtensionFunction::ResponseAction NotesEmptyTrashFunction::Run() {
  bool success = false;

  Notes_Model* model = GetNotesModel(this);
  Notes_Node* trash_node = model->trash_node();
  if (trash_node) {
    while (trash_node->child_count()) {
      Notes_Node* node = trash_node->GetChild(0);
      int64_t removed_node_id = node->id();
      model->Remove(trash_node, 0);
      SendRemoved(browser_context(), removed_node_id, trash_node->id(), 0);
    }
    success = true;
  }
  return RespondNow(
      ArgumentList(vivaldi::notes::EmptyTrash::Results::Create(success)));
}

}  // namespace extensions
