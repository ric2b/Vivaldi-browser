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

namespace extensions {

const char noteNotFoundStr[] = "Note not found.";

NotesEventRouter::NotesEventRouter(Profile* profile)
    : browser_context_(profile),
      model_(NotesModelFactory::GetForBrowserContext(profile)) {
  model_->AddObserver(this);
}

NotesEventRouter::~NotesEventRouter() {
  model_->RemoveObserver(this);
}

void NotesEventRouter::ExtensiveNotesChangesBeginning(Notes_Model* model) {
  DispatchEvent(vivaldi::notes::OnImportBegan::kEventName,
                vivaldi::notes::OnImportBegan::Create());
}

void NotesEventRouter::ExtensiveNotesChangesEnded(Notes_Model* model) {
  DispatchEvent(vivaldi::notes::OnImportEnded::kEventName,
                vivaldi::notes::OnImportEnded::Create());
}

// Helper to actually dispatch an event to extension listeners.
void NotesEventRouter::DispatchEvent(
    const std::string& event_name,
    std::unique_ptr<base::ListValue> event_args) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    event_router->BroadcastEvent(base::WrapUnique(
        new extensions::Event(extensions::events::VIVALDI_EXTENSION_EVENT,
                              event_name, std::move(event_args))));
  }
}

void BroadcastEvent(const std::string& eventname,
                    std::unique_ptr<base::ListValue> args,
                    content::BrowserContext* context) {
  std::unique_ptr<extensions::Event> event(
      new extensions::Event(extensions::events::VIVALDI_EXTENSION_EVENT,
                            eventname, std::move(args), context));
  EventRouter* event_router = EventRouter::Get(context);
  if (event_router) {
    event_router->BroadcastEvent(std::move(event));
  }
}

NotesAPI::NotesAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this,
                                 vivaldi::notes::OnImportBegan::kEventName);
  event_router->RegisterObserver(this,
                                 vivaldi::notes::OnImportEnded::kEventName);
}

NotesAPI::~NotesAPI() {}

void NotesAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<NotesAPI> >::
    DestructorAtExit g_factory_notes = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<NotesAPI>* NotesAPI::GetFactoryInstance() {
  return g_factory_notes.Pointer();
}

void NotesAPI::OnListenerAdded(const EventListenerInfo& details) {
  notes_event_router_.reset(
      new NotesEventRouter(Profile::FromBrowserContext(browser_context_)));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

std::unique_ptr<NoteAttachment> CreateNoteAttachment(
    const ::vivaldi::NoteAttachment& attachment) {
  auto note_attachment = std::make_unique<NoteAttachment>();

  note_attachment->content = attachment.content();
  note_attachment->checksum =
      std::make_unique<std::string>(attachment.checksum());
  return note_attachment;
}

std::unique_ptr<std::vector<NoteAttachment>> CreateNoteAttachments(
    const ::vivaldi::NoteAttachments& attachments) {
  auto new_attachments = std::make_unique<std::vector<NoteAttachment>>();
  for (const auto& attachment : attachments) {
    new_attachments->push_back(
        std::move(*CreateNoteAttachment(attachment.second)));
  }

  return new_attachments;
}

std::unique_ptr<NoteTreeNode> CreateTreeNode(Notes_Node* node) {
  std::unique_ptr<NoteTreeNode> notes_tree_node =
      std::make_unique<NoteTreeNode>();

  notes_tree_node->id = base::Int64ToString(node->id());

  const Notes_Node* parent = node->parent();
  if (parent) {
    notes_tree_node->parent_id.reset(
        new std::string(base::Int64ToString(parent->id())));
    notes_tree_node->index.reset(new int(parent->GetIndexOf(node)));
  }
  notes_tree_node->trash.reset(new bool(node->is_trash()));

  notes_tree_node->separator.reset(new bool(node->is_separator()));

  notes_tree_node->title.reset(
      new std::string(base::UTF16ToUTF8(node->GetTitle())));
  notes_tree_node->content.reset(
      new std::string(base::UTF16ToUTF8(node->GetContent())));

  if (node->GetURL().is_valid()) {
    notes_tree_node->url.reset(new std::string(node->GetURL().spec()));
  }

  notes_tree_node->attachments = CreateNoteAttachments(node->GetAttachments());

  // Javascript Date wants milliseconds since the epoch, ToDoubleT is seconds.
  double timedouble = node->GetCreationTime().ToDoubleT();
  notes_tree_node->date_added.reset(new double(floor(timedouble * 1000)));

  if (node->is_folder()) {
    std::vector<NoteTreeNode> children;
    for (int i = 0; i < node->child_count(); ++i) {
      Notes_Node* child = node->GetChild(i);
      std::unique_ptr<NoteTreeNode> child_node(CreateTreeNode(child));
      children.push_back(std::move(*child_node));
    }
    notes_tree_node->children.reset(
        new std::vector<NoteTreeNode>(std::move(children)));
  }
  return notes_tree_node;
}

///// TODO MOVE TO SEPARATE FILE  ^^^

NotesGetFunction::NotesGetFunction() {}

Notes_Node* NotesAsyncFunction::GetNodeFromId(Notes_Node* node, int64_t id) {
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

Notes_Model* NotesAsyncFunction::GetNotesModel() {
  return NotesModelFactory::GetForBrowserContext(GetProfile());
}

bool NotesGetFunction::RunAsync() {
  std::unique_ptr<vivaldi::notes::Get::Params> params(
      vivaldi::notes::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::vector<NoteTreeNode> notes;
  Notes_Model* model = NotesModelFactory::GetForBrowserContext(GetProfile());
  Notes_Node* root = model->root_node();
  if (params->id_or_id_list.as_strings) {
    std::vector<std::string>& ids = *params->id_or_id_list.as_strings;
    size_t count = ids.size();
    EXTENSION_FUNCTION_VALIDATE(count > 0);
    for (size_t i = 0; i < count; ++i) {
      int64_t idval;
      base::StringToInt64(ids[i], &idval);
      Notes_Node* node = GetNodeFromId(root, idval);
      if (!node) {
        error_ = noteNotFoundStr;
        SendResponse(false);
        return false;
      }
      std::unique_ptr<NoteTreeNode> note(CreateTreeNode(node));
      notes.push_back(std::move(*note));
    }
  } else {
    int64_t idval;
    base::StringToInt64(*params->id_or_id_list.as_string, &idval);
    Notes_Node* node = GetNodeFromId(root, idval);
    if (!node) {
      error_ = noteNotFoundStr;
      SendResponse(false);
      return false;
    }
    std::unique_ptr<NoteTreeNode> note(CreateTreeNode(node));
    notes.push_back(std::move(*note));
  }

  results_ = vivaldi::notes::Get::Results::Create(notes);

  SendResponse(true);
  return true;
}

NotesGetFunction::~NotesGetFunction() {}

NotesGetTreeFunction::NotesGetTreeFunction() {}

bool NotesGetTreeFunction::RunAsync() {
  Notes_Model* model = NotesModelFactory::GetForBrowserContext(GetProfile());

  // If the model has not loaded yet wait until it does and do the work then.
  if (!model->loaded()) {
    model->AddObserver(this);
    notes_model_ = model;
    AddRef();  // Balanced in NotesModelLoaded and NotesModelBeingDeleted.
  } else {
    SendGetTreeResponse(model);
  }

  return true;
}

bool NotesGetTreeFunction::SendGetTreeResponse(Notes_Model* model) {
  std::vector<NoteTreeNode> notes;
  Notes_Node* root = model->main_node();
  std::unique_ptr<NoteTreeNode> new_note(CreateTreeNode(root));
  std::unique_ptr<NoteTreeNode> trash_note(CreateTreeNode(model->trash_node()));
  new_note->children->push_back(std::move(*trash_note));
  if (new_note->children.get()->size()) {  // Do not return root.
    notes.push_back(std::move(*new_note));
  }
  results_ = vivaldi::notes::GetTree::Results::Create(notes);
  SendResponse(true);
  return true;
}

NotesGetTreeFunction::~NotesGetTreeFunction() {
  if (notes_model_) {
    notes_model_->RemoveObserver(this);
  }
  if (!did_respond()) {
    SendResponse(false);
  }
}

void NotesGetTreeFunction::NotesModelLoaded(Notes_Model* model,
                                            bool ids_reassigned) {
  SendGetTreeResponse(model);
  model->RemoveObserver(this);
  notes_model_ = nullptr;
  Release();
}

void NotesGetTreeFunction::NotesModelBeingDeleted(Notes_Model* model) {
  model->RemoveObserver(this);
  notes_model_ = nullptr;
  Release();
}

NotesCreateFunction::NotesCreateFunction() {}

bool NotesCreateFunction::RunAsync() {
  std::unique_ptr<vivaldi::notes::Create::Params> params(
      vivaldi::notes::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Notes_Model* model = GetNotesModel();

  Notes_Node* parent = NULL;

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

  if (params->note.parent_id.get()) {
    int64_t idval;
    base::StringToInt64(*params->note.parent_id.get(), &idval);
    parent = GetNodeFromId(model->root_node(), idval);
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

  std::unique_ptr<vivaldi::notes::NoteTreeNode> treenode(
      CreateTreeNode(newnode_ptr));

  results_ = vivaldi::notes::Create::Results::Create(*treenode.get());

  std::unique_ptr<base::ListValue> args = vivaldi::notes::OnCreated::Create(
      base::Int64ToString(newnode_ptr->id()), *std::move(treenode));

  BroadcastEvent(vivaldi::notes::OnCreated::kEventName, std::move(args),
                 context_);

  SendResponse(true);
  return true;
}

NotesCreateFunction::~NotesCreateFunction() {}

NotesUpdateFunction::NotesUpdateFunction() {}

bool NotesUpdateFunction::RunAsync() {
  std::unique_ptr<vivaldi::notes::Update::Params> params(
      vivaldi::notes::Update::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Notes_Model* model = GetNotesModel();
  int64_t idval;
  base::StringToInt64(params->id, &idval);
  Notes_Node* node = GetNodeFromId(model->root_node(), idval);
  if (!node) {
    error_ = noteNotFoundStr;
    SendResponse(false);
    return false;
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

  std::unique_ptr<vivaldi::notes::NoteTreeNode> ret(CreateTreeNode(node));

  results_ = vivaldi::notes::Create::Results::Create(*ret);

  SendResponse(true);

  std::unique_ptr<base::ListValue> args = vivaldi::notes::OnChanged::Create(
      base::Int64ToString(node->id()), changeinfo);

  BroadcastEvent(vivaldi::notes::OnChanged::kEventName, std::move(args),
                 context_);

  return model->SaveNotes();
}

NotesUpdateFunction::~NotesUpdateFunction() {}

NotesRemoveFunction::NotesRemoveFunction() {}

bool NotesRemoveFunction::RunAsync() {
  std::unique_ptr<vivaldi::notes::Remove::Params> params(
      vivaldi::notes::Remove::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int64_t id;
  base::StringToInt64(params->id, &id);

  Notes_Model* model = GetNotesModel();

  Notes_Node* root = model->root_node();
  Notes_Node* node = GetNodeFromId(root, id);
  if (!node) {
    error_ = noteNotFoundStr;
    SendResponse(false);
    return false;
  }
  Notes_Node* parent = node->parent();
  if (!parent) {
    parent = root;
  }
  Notes_Node* trash_node = model->trash_node();
  if (trash_node == node) {
    // Trying to delete trash, should not happen.
    NOTREACHED();
    return true;
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
    model->Move(node, trash_node, 0);

    SendResponse(true);

    std::unique_ptr<vivaldi::notes::NoteTreeNode> ret(CreateTreeNode(node));
    results_ = vivaldi::notes::Move::Results::Create(*ret);

    vivaldi::notes::OnMoved::MoveInfo moveInfo;

    moveInfo.index = 0;
    moveInfo.old_index = oldIndex;

    moveInfo.parent_id = base::Int64ToString(trash_node->id());
    moveInfo.old_parent_id = base::Int64ToString(old_parent->id());

    std::unique_ptr<base::ListValue> args = vivaldi::notes::OnMoved::Create(
        base::Int64ToString(node->id()), moveInfo);

    BroadcastEvent(vivaldi::notes::OnMoved::kEventName, std::move(args),
                   context_);

  } else {
    if (!model->Remove(parent, indexofdeleted)) {
      error_ = noteNotFoundStr;
      SendResponse(false);
      return false;
    }
    SendResponse(true);

    vivaldi::notes::OnRemoved::RemoveInfo info;

    info.parent_id = parent->id();
    info.index = indexofdeleted;

    std::unique_ptr<base::ListValue> args =
        vivaldi::notes::OnRemoved::Create(base::Int64ToString(id), info);

    BroadcastEvent(vivaldi::notes::OnRemoved::kEventName, std::move(args),
                   context_);
  }
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
    ui::TreeNodeIterator<Notes_Node> iterator(
        NotesModelFactory::GetForBrowserContext(GetProfile())->root_node());

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
        std::unique_ptr<vivaldi::notes::NoteTreeNode> note(
            CreateTreeNode(node));
        search_result.push_back(std::move(*note));
      }
    }
  }

  results_ = vivaldi::notes::Search::Results::Create(search_result);
  SendResponse(true);
  return true;
}

NotesSearchFunction::~NotesSearchFunction() {}

NotesMoveFunction::NotesMoveFunction() {}

bool NotesMoveFunction::RunAsync() {
  std::unique_ptr<vivaldi::notes::Move::Params> params(
      vivaldi::notes::Move::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Notes_Model* model = NotesModelFactory::GetForBrowserContext(GetProfile());
  Notes_Node* root = model->root_node();

  int64_t id;
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
    int64_t parentId;
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

  bool moved = model->Move(node, parent, index);
  DCHECK(moved);
  if (!moved) {
    // This will happen if we attempt to move a folder into a subfolder of its
    // own. Should never happen (missing test in JS), but better be on the safe
    // side as a reply will mess up the displayed data.
    return false;
  }

  std::unique_ptr<vivaldi::notes::NoteTreeNode> ret(CreateTreeNode(node));
  results_ = vivaldi::notes::Move::Results::Create(*ret);

  vivaldi::notes::OnMoved::MoveInfo moveInfo;

  moveInfo.index = index;
  moveInfo.old_index = oldIndex;

  moveInfo.parent_id = base::Int64ToString(parent->id());
  moveInfo.old_parent_id = aid;

  std::unique_ptr<base::ListValue> args = vivaldi::notes::OnMoved::Create(
      base::Int64ToString(node->id()), moveInfo);

  BroadcastEvent(vivaldi::notes::OnMoved::kEventName, std::move(args),
                 context_);

  SendResponse(true);
  return true;
}

NotesMoveFunction::~NotesMoveFunction() {}

NotesEmptyTrashFunction::NotesEmptyTrashFunction() {}

bool NotesEmptyTrashFunction::RunAsync() {
  bool success = false;

  Notes_Model* model = GetNotesModel();
  Notes_Node* trash_node = model->trash_node();
  if (trash_node) {
    while (trash_node->child_count()) {
      Notes_Node* node = trash_node->GetChild(0);
      int64_t removed_node_id = node->id();
      model->Remove(trash_node, 0);

      vivaldi::notes::OnRemoved::RemoveInfo info;
      info.parent_id = base::Int64ToString(trash_node->id());
      info.index = 0;
      std::unique_ptr<base::ListValue> args = vivaldi::notes::OnRemoved::Create(
          base::Int64ToString(removed_node_id), info);

      BroadcastEvent(vivaldi::notes::OnRemoved::kEventName, std::move(args),
                     context_);
    }
    success = true;
  }
  results_ = vivaldi::notes::EmptyTrash::Results::Create(success);
  SendResponse(true);
  return true;
}

NotesEmptyTrashFunction::~NotesEmptyTrashFunction() {}

}  // namespace extensions
