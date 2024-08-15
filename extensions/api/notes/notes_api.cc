// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/notes/notes_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/i18n/string_search.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "components/notes/note_node.h"
#include "components/notes/notes_factory.h"
#include "components/notes/notes_model.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/schema/notes.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/base/models/tree_node_iterator.h"

using extensions::vivaldi::notes::NoteTreeNode;
using vivaldi::NoteNode;
using vivaldi::NotesModel;
using vivaldi::NotesModelFactory;

namespace extensions {

namespace {

vivaldi::notes::NodeType ToJSAPINodeType(NoteNode::Type type) {
  switch (type) {
    case NoteNode::NOTE:
      return vivaldi::notes::NodeType::kNote;
    case NoteNode::FOLDER:
      return vivaldi::notes::NodeType::kFolder;
    case NoteNode::SEPARATOR:
      return vivaldi::notes::NodeType::kSeparator;
    case NoteNode::ATTACHMENT:
      return vivaldi::notes::NodeType::kAttachment;
    case NoteNode::MAIN:
      return vivaldi::notes::NodeType::kMain;
    case NoteNode::OTHER:
      return vivaldi::notes::NodeType::kOther;
    case NoteNode::TRASH:
      return vivaldi::notes::NodeType::kTrash;
  }
}

std::optional<NoteNode::Type> FromJSAPINodeType(vivaldi::notes::NodeType type) {
  switch (type) {
    case vivaldi::notes::NodeType::kNote:
      return NoteNode::NOTE;
    case vivaldi::notes::NodeType::kFolder:
      return NoteNode::FOLDER;
    case vivaldi::notes::NodeType::kSeparator:
      return NoteNode::SEPARATOR;
    case vivaldi::notes::NodeType::kAttachment:
      return NoteNode::ATTACHMENT;
    case vivaldi::notes::NodeType::kMain:
      return NoteNode::MAIN;
    case vivaldi::notes::NodeType::kOther:
      return NoteNode::OTHER;
    case vivaldi::notes::NodeType::kTrash:
      return NoteNode::TRASH;
    default:
      return std::nullopt;
  }
}

NoteTreeNode MakeTreeNode(const NoteNode* node) {
  NoteTreeNode notes_tree_node;

  notes_tree_node.id = base::NumberToString(node->id());

  const NoteNode* parent = node->parent();
  if (parent) {
    notes_tree_node.parent_id = base::NumberToString(parent->id());
    notes_tree_node.index = parent->GetIndexOf(node);
  }
  notes_tree_node.type = ToJSAPINodeType(node->type());

  notes_tree_node.title = base::UTF16ToUTF8(node->GetTitle());

  if (node->is_note() || node->is_attachment()) {
    notes_tree_node.content = base::UTF16ToUTF8(node->GetContent());

    if (node->GetURL().is_valid()) {
      notes_tree_node.url = node->GetURL().spec();
    }
  }

  // Javascript Date wants milliseconds since the epoch.
  notes_tree_node.date_added =
      node->GetCreationTime().InMillisecondsFSinceUnixEpoch();
  notes_tree_node.date_modified =
      node->GetLastModificationTime().InMillisecondsFSinceUnixEpoch();

  if (node->is_folder() || node->is_note()) {
    notes_tree_node.children.emplace();
    std::vector<NoteTreeNode> children;
    for (auto& it : node->children()) {
      notes_tree_node.children->push_back(MakeTreeNode(it.get()));
    }
  }
  return notes_tree_node;
}

NotesModel* GetNotesModel(ExtensionFunction* fun) {
  return NotesModelFactory::GetForBrowserContext(fun->browser_context());
}

const NoteNode* ParseNoteId(NotesModel* model,
                            const std::string& id_str,
                            std::string* error) {
  int64_t id;
  if (!base::StringToInt64(id_str, &id)) {
    *error = "Note id is not a number - " + id_str;
    return nullptr;
  }
  const NoteNode* node = model->GetNoteNodeByID(id);
  if (!node) {
    *error = "No note with id " + id_str;
    return nullptr;
  }
  return node;
}

}  // namespace

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<NotesAPI>>::DestructorAtExit g_factory_notes =
    LAZY_INSTANCE_INITIALIZER;

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

void NotesAPI::NotesNodeMoved(const NoteNode* old_parent,
                              size_t old_index,
                              const NoteNode* new_parent,
                              size_t new_index) {
  vivaldi::notes::OnMoved::MoveInfo move_info;

  move_info.index = new_index;
  move_info.old_index = old_index;
  move_info.parent_id = base::NumberToString(new_parent->id());
  move_info.old_parent_id = base::NumberToString(old_parent->id());

  const NoteNode* node = new_parent->children()[new_index].get();

  ::vivaldi::BroadcastEvent(vivaldi::notes::OnMoved::kEventName,
                            vivaldi::notes::OnMoved::Create(
                                base::NumberToString(node->id()), move_info),
                            browser_context_);
}

void NotesAPI::NotesNodeAdded(const NoteNode* parent, size_t index) {
  const NoteNode* new_node = parent->children()[index].get();
  vivaldi::notes::NoteTreeNode treenode = MakeTreeNode(new_node);

  ::vivaldi::BroadcastEvent(vivaldi::notes::OnCreated::kEventName,
                            vivaldi::notes::OnCreated::Create(
                                base::NumberToString(new_node->id()), treenode),
                            browser_context_);
}
void NotesAPI::NotesNodeRemoved(const NoteNode* parent,
                                size_t old_index,
                                const NoteNode* node,
                                const base::Location& location) {
  vivaldi::notes::OnRemoved::RemoveInfo info;

  info.parent_id = base::NumberToString(parent->id());
  info.index = old_index;

  ::vivaldi::BroadcastEvent(
      vivaldi::notes::OnRemoved::kEventName,
      vivaldi::notes::OnRemoved::Create(base::NumberToString(node->id()), info),
      browser_context_);
}

void NotesAPI::NotesNodeChanged(const NoteNode* node) {
  vivaldi::notes::OnChanged::NoteAfterChange note_after_change;
  note_after_change.title = base::UTF16ToUTF8(node->GetTitle());
  note_after_change.date_modified =
      node->GetLastModificationTime().InMillisecondsFSinceUnixEpoch();
  if (node->is_note()) {
    note_after_change.content = base::UTF16ToUTF8(node->GetContent());
    note_after_change.url = node->GetURL().spec();
  }

  ::vivaldi::BroadcastEvent(
      vivaldi::notes::OnChanged::kEventName,
      vivaldi::notes::OnChanged::Create(base::NumberToString(node->id()),
                                        note_after_change),
      browser_context_);
}

void NotesAPI::ExtensiveNotesChangesBeginning() {
  ::vivaldi::BroadcastEvent(vivaldi::notes::OnImportBegan::kEventName,
                            vivaldi::notes::OnImportBegan::Create(),
                            browser_context_);
}

void NotesAPI::ExtensiveNotesChangesEnded() {
  ::vivaldi::BroadcastEvent(vivaldi::notes::OnImportEnded::kEventName,
                            vivaldi::notes::OnImportEnded::Create(),
                            browser_context_);
}

ExtensionFunction::ResponseAction NotesGetFunction::Run() {
  std::optional<vivaldi::notes::Get::Params> params(
      vivaldi::notes::Get::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<NoteTreeNode> notes;
  NotesModel* model = GetNotesModel(this);
  std::string error;
  if (params->id_or_id_list.as_strings) {
    std::vector<std::string>& ids = *params->id_or_id_list.as_strings;
    size_t count = ids.size();
    EXTENSION_FUNCTION_VALIDATE(count > 0);
    for (size_t i = 0; i < count; ++i) {
      const NoteNode* node = ParseNoteId(model, ids[i], &error);
      if (!node) {
        return RespondNow(Error(error));
      }
      notes.push_back(MakeTreeNode(node));
    }
  } else {
    const NoteNode* node =
        ParseNoteId(model, *params->id_or_id_list.as_string, &error);
    if (!node) {
      return RespondNow(Error(error));
    }
    notes.push_back(MakeTreeNode(node));
  }
  return RespondNow(ArgumentList(vivaldi::notes::Get::Results::Create(notes)));
}

ExtensionFunction::ResponseAction NotesGetTreeFunction::Run() {
  NotesModel* model = GetNotesModel(this);

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

void NotesGetTreeFunction::SendGetTreeResponse(NotesModel* model) {
  std::vector<NoteTreeNode> notes;
  const NoteNode* root = model->main_node();
  NoteTreeNode new_note = MakeTreeNode(root);
  new_note.children->push_back(MakeTreeNode(model->trash_node()));
  // After the above push the condition is always true
  // if (new_note.children->size()) {  // Do not return root.
  notes.push_back(std::move(new_note));
  //}
  Respond(ArgumentList(vivaldi::notes::GetTree::Results::Create(notes)));
}

void NotesGetTreeFunction::NotesModelLoaded(bool ids_reassigned) {
  NotesModel* model = GetNotesModel(this);
  SendGetTreeResponse(model);
  model->RemoveObserver(this);
  Release();
}

void NotesGetTreeFunction::NotesModelBeingDeleted() {
  NotesModel* model = GetNotesModel(this);
  Respond(Error("NotesModelBeingDeleted"));
  model->RemoveObserver(this);
  Release();
}

ExtensionFunction::ResponseAction NotesCreateFunction::Run() {
  std::optional<vivaldi::notes::Create::Params> params(
      vivaldi::notes::Create::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  NotesModel* model = GetNotesModel(this);

  // default to note
  NoteNode::Type type = *FromJSAPINodeType(params->note.type);

  if (type == NoteNode::FOLDER || type == NoteNode::SEPARATOR) {
    if (params->note.content)
      return RespondNow(Error(
          "Note content can only be set for regular notes or attachments"));
    if (params->note.url)
      return RespondNow(Error("Note URL can only be set for regular notes"));
  }

  // Lots of optionals, make sure to check for the contents.
  std::u16string title;
  if (params->note.title) {
    title = base::UTF8ToUTF16(*params->note.title);
  }

  std::string content;
  if (params->note.content) {
    content = *params->note.content;
  }

  GURL url;
  if (params->note.url) {
    url = GURL(*params->note.url);
  }

  std::optional<base::Time> creation_time;
  if (params->note.date) {
    creation_time =
        base::Time::FromMillisecondsSinceUnixEpoch(*params->note.date);
  }

  std::optional<base::Time> last_modified_time;
  if (params->note.lastmod) {
    last_modified_time =
        base::Time::FromMillisecondsSinceUnixEpoch(*params->note.lastmod);
  }

  const NoteNode* parent = nullptr;
  if (params->note.parent_id) {
    std::string error;
    parent = ParseNoteId(model, *params->note.parent_id, &error);
    if (!parent) {
      return RespondNow(Error(error));
    }
  }

  if (!parent || parent == model->root_node()) {
    parent = model->main_node();
  }

  int64_t maxIndex = parent->children().size();
  int64_t newIndex = maxIndex;
  if (parent == model->main_node()) {
    if (params->note.index) {
      newIndex = *params->note.index;
      if (newIndex > maxIndex) {
        newIndex = maxIndex;
      }
    }
  } else {
    if (params->note.index) {
      newIndex = *params->note.index;
    }
  }

  const NoteNode* new_node = nullptr;
  switch (type) {
    case NoteNode::NOTE:
      new_node = model->AddNote(parent, newIndex, title, url,
                                base::UTF8ToUTF16(content), creation_time,
                                last_modified_time);
      break;
    case NoteNode::SEPARATOR:
      new_node = model->AddSeparator(parent, newIndex, title, creation_time);
      break;
    case NoteNode::ATTACHMENT: {
      std::optional<std::vector<uint8_t>> decoded_content =
          base::Base64Decode(content);
      if (!decoded_content || decoded_content->empty()) {
        new_node = model->AddAttachmentFromChecksum(
            parent, newIndex, title, url, content, creation_time);
      } else {
        new_node = model->AddAttachment(parent, newIndex, title, url,
                                        *decoded_content, creation_time);
      }
      break;
    }
    case NoteNode::FOLDER:
      new_node = model->AddFolder(parent, newIndex, title, creation_time,
                                  last_modified_time);
      break;
    default:
      NOTREACHED();
  }

  vivaldi::notes::NoteTreeNode treenode = MakeTreeNode(new_node);

  return RespondNow(
      ArgumentList(vivaldi::notes::Create::Results::Create(treenode)));
}

ExtensionFunction::ResponseAction NotesUpdateFunction::Run() {
  std::optional<vivaldi::notes::Update::Params> params(
      vivaldi::notes::Update::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  NotesModel* model = GetNotesModel(this);
  std::string error;
  const NoteNode* node = ParseNoteId(model, params->id, &error);
  if (!node) {
    return RespondNow(Error(error));
  }

  if (model->is_permanent_node(node)) {
    return RespondNow(Error("Cannot modify permanent nodes"));
  }

  if (!node->is_note() && !node->is_attachment()) {
    if (params->changes.content)
      return RespondNow(
          Error("Note content can only be set for regular notes"));
    if (params->changes.url)
      return RespondNow(
          Error("Note URL can only be set for regular notes or attachments"));
  }

  if (node->is_attachment() && params->changes.content)
    return RespondNow(Error("Attachment content can not be modified"));

  // All fields are optional.
  if (params->changes.title) {
    std::u16string title = base::UTF8ToUTF16(*params->changes.title);
    model->SetTitle(node, title);
  }

  if (params->changes.content) {
    std::u16string content = base::UTF8ToUTF16(*params->changes.content);
    model->SetContent(node, content);
  }

  if (params->changes.url) {
    GURL url(*params->changes.url);
    model->SetURL(node, url);
  }

  return RespondNow(ArgumentList(
      vivaldi::notes::Create::Results::Create(MakeTreeNode(node))));
}

ExtensionFunction::ResponseAction NotesRemoveFunction::Run() {
  std::optional<vivaldi::notes::Remove::Params> params(
      vivaldi::notes::Remove::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  NotesModel* model = GetNotesModel(this);
  std::string error;
  const NoteNode* node = ParseNoteId(model, params->id, &error);
  if (!node) {
    return RespondNow(Error(error));
  }

  if (model->is_permanent_node(node)) {
    return RespondNow(Error("Cannot modify permanent nodes"));
  }

  const NoteNode* parent = node->parent();
  if (!parent) {
    parent = model->root_node();
  }

  const NoteNode* trash_node = model->trash_node();

  bool move_to_trash = true;
  if (node->is_separator() || node->is_attachment()) {
    move_to_trash = false;
  } else {
    const NoteNode* tmp = node;
    while (!tmp->is_root()) {
      if (tmp->parent() == trash_node) {
        move_to_trash = false;
        break;
      }
      tmp = tmp->parent();
    }
  }

  if (move_to_trash) {
    model->Move(node, trash_node, 0);
    return RespondNow(ArgumentList(vivaldi::notes::Remove::Results::Create()));
  } else {
    model->Remove(node, FROM_HERE);
    return RespondNow(ArgumentList(vivaldi::notes::Remove::Results::Create()));
  }
}

ExtensionFunction::ResponseAction NotesSearchFunction::Run() {
  std::optional<vivaldi::notes::Search::Params> params(
      vivaldi::notes::Search::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

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

  std::u16string needle = base::UTF8ToUTF16(params->query.substr(offset));
  if (needle.length() > 0) {
    ui::TreeNodeIterator<const NoteNode> iterator(
        GetNotesModel(this)->root_node());

    while (iterator.has_next()) {
      const NoteNode* node = iterator.Next();
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
  std::optional<vivaldi::notes::Move::Params> params(
      vivaldi::notes::Move::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  NotesModel* model = GetNotesModel(this);

  std::string error;
  const NoteNode* node = ParseNoteId(model, params->id, &error);
  if (!node) {
    return RespondNow(Error(error));
  }

  if (model->is_permanent_node(node)) {
    return RespondNow(Error("Cannot modify permanent nodes"));
  }

  const NoteNode* parent;
  if (!params->destination.parent_id) {
    // Optional, defaults to current parent.
    parent = node->parent();
  } else {
    parent = ParseNoteId(model, *params->destination.parent_id, &error);
    if (!parent) {
      return RespondNow(Error(error));
    }
    if (model->is_root_node(node)) {
      return RespondNow(Error("Node cannot be made a child of root node"));
    }
  }

  if (node->is_attachment() && !parent->is_note()) {
    return RespondNow(Error("Attachments can only be the children of notes."));
  }

  size_t index;
  if (params->destination.index) {  // Optional (defaults to end).
    index = *params->destination.index;
    if (!model->IsValidIndex(parent, index, true)) {
      // Todo move to constant
      return RespondNow(Error("Index out of bounds."));
    }
  } else {
    index = parent->children().size();
  }

  if (parent->HasAncestor(node)) {
    return RespondNow(Error("Node cannot be made a descendant of itself."));
  }

  model->Move(node, parent, index);

  return RespondNow(
      ArgumentList(vivaldi::notes::Move::Results::Create(MakeTreeNode(node))));
}

ExtensionFunction::ResponseAction NotesEmptyTrashFunction::Run() {
  bool success = false;

  NotesModel* model = GetNotesModel(this);
  const NoteNode* trash_node = model->trash_node();
  if (trash_node) {
    while (!trash_node->children().empty()) {
      model->Remove(trash_node->children()[0].get(), FROM_HERE);
    }
    success = true;
  }
  return RespondNow(
      ArgumentList(vivaldi::notes::EmptyTrash::Results::Create(success)));
}

ExtensionFunction::ResponseAction NotesBeginImportFunction::Run() {
  NotesModel* model = GetNotesModel(this);
  DCHECK(model);

  model->BeginExtensiveChanges();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction NotesEndImportFunction::Run() {
  NotesModel* model = GetNotesModel(this);
  DCHECK(model);

  model->EndExtensiveChanges();
  return RespondNow(NoArguments());
}

}  // namespace extensions
