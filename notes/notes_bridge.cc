// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved
// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "notes_bridge.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <android/log.h>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/stack.h"
#include "base/containers/stack_container.h"
#include "base/i18n/string_compare.h"
#include "notes/notes_factory.h"
#include "chrome/android/chrome_jni_headers/NotesBridge_jni.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "notes/notes_model.h"
#include "notes/notes_factory.h"
#include "notes/note_id.h"
#include "notes/note_type.h"

#include "components/prefs/pref_service.h"
#include "components/query_parser/query_parser.h"
#include "components/signin/public/identity_manager/identity_manager.h"

#include "components/undo/undo_manager.h"
#include "content/public/browser/browser_thread.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ToJavaIntArray;
using notes::android::JavaNoteIdCreateNoteId;
using notes::android::JavaNoteIdGetId;
using notes::android::JavaNoteIdGetType;
using vivaldi::Notes_Node;
using vivaldi::NotesModelFactory;
using content::BrowserThread;

namespace {

class NoteTitleComparer {
 public:
  explicit NoteTitleComparer(NotesBridge* notes_bridge,
                             const icu::Collator* collator)
      : notes_bridge_(notes_bridge),
        collator_(collator) {}

  bool operator()(const Notes_Node* lhs, const Notes_Node* rhs) {
    if (collator_) {
      return base::i18n::CompareString16WithCollator(
                 *collator_, notes_bridge_->GetContent(lhs),
                 notes_bridge_->GetContent(rhs)) == UCOL_LESS;
    } else {
      return lhs->GetContent() < rhs->GetContent();
    }
  }

private:
  NotesBridge* notes_bridge_;  // weak
  const icu::Collator* collator_;
};

std::unique_ptr<icu::Collator> GetICUCollator() {
  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator_;
  collator_.reset(icu::Collator::createInstance(error));
  if (U_FAILURE(error))
    collator_.reset(NULL);
  return collator_;
}

}  // namespace

NotesBridge::NotesBridge(JNIEnv* env,
                         const JavaRef<jobject>& obj,
                         const JavaRef<jobject>& j_profile)
    : weak_java_ref_(env, obj),
      notes_model_(NULL){
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  profile_ = ProfileAndroid::FromProfileAndroid(j_profile);
  notes_model_ = NotesModelFactory::GetForBrowserContext(profile_);

  // Registers the notifications we are interested.
  notes_model_->AddObserver(this);

  //pref_change_registrar_.Init(profile_->GetPrefs());

  NotifyIfDoneLoading();

  // Since a sync or import could have started before this class is
  // initialized, we need to make sure that our initial state is
  // up to date.
  if (notes_model_->IsDoingExtensiveChanges())
    ExtensiveNoteChangesBeginning(notes_model_);
}

NotesBridge::~NotesBridge() {
  notes_model_->RemoveObserver(this);
}

void NotesBridge::Destroy(JNIEnv*, const JavaParamRef<jobject>&) {
  delete this;
}

static jlong JNI_NotesBridge_Init(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  const JavaParamRef<jobject>& j_profile) {
  NotesBridge* delegate = new NotesBridge(env, obj, j_profile);
  return reinterpret_cast<intptr_t>(delegate);
}

jboolean NotesBridge::IsEditNotesEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return IsEditNotesEnabled();
}

ScopedJavaLocalRef<jobject> NotesBridge::GetNoteByID(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong id,
    jint type) {
  DCHECK(IsLoaded());
  const Notes_Node* node = GetNodeByID(id, type);
  return node ? CreateJavaNote(node) : ScopedJavaLocalRef<jobject>();
}

bool NotesBridge::IsDoingExtensiveChanges(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return notes_model_->IsDoingExtensiveChanges();
}

void NotesBridge::GetPermanentNodeIDs(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_result_obj) {
  DCHECK(IsLoaded());

  base::StackVector<const Notes_Node*, 8> permanent_nodes;

  // Save all the permanent nodes.
  const Notes_Node* root_node = notes_model_->root_node();
  permanent_nodes->push_back(root_node);
  for (const auto& child : root_node->children())
    permanent_nodes->push_back(child.get());

  // Write the permanent nodes to |j_result_obj|.
  for (base::StackVector<const Notes_Node*, 8>::ContainerType::const_iterator
           it = permanent_nodes->begin();
       it != permanent_nodes->end();
       ++it) {
    if (*it != NULL) {
      Java_NotesBridge_addToNoteIdList(
          env, j_result_obj, (*it)->id(), GetNoteType(*it));
    }
  }
}

void NotesBridge::GetTopLevelFolderParentIDs(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_result_obj) {
  Java_NotesBridge_addToNoteIdList(
      env, j_result_obj, notes_model_->root_node()->id(),
      GetNoteType(notes_model_->root_node()));
}

void NotesBridge::GetTopLevelFolderIDs(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean get_special,
    jboolean get_normal,
    const JavaParamRef<jobject>& j_result_obj) {
  DCHECK(IsLoaded());
  std::vector<const Notes_Node*> top_level_folders;

  std::size_t special_count = top_level_folders.size();

  if (get_normal) {
    // Vivaldi: trash folder added
    DCHECK_EQ(/*5u*/6u, notes_model_->root_node()->children().size());
    const Notes_Node* main_node = notes_model_->main_node();
    for (const auto& child : main_node->children()) {
      const Notes_Node* node = child.get();
      if (node->is_folder()) {
        top_level_folders.push_back(node);
      }
    }

    std::unique_ptr<icu::Collator> collator = GetICUCollator();
    std::stable_sort(top_level_folders.begin() + special_count,
                     top_level_folders.end(),
                     NoteTitleComparer(this, collator.get()));
  }

  for (std::vector<const Notes_Node*>::const_iterator it =
      top_level_folders.begin(); it != top_level_folders.end(); ++it) {
    Java_NotesBridge_addToNoteIdList(env,
                                             j_result_obj,
                                             (*it)->id(),
                                             GetNoteType(*it));
  }
}

void NotesBridge::GetAllFoldersWithDepths(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_folders_obj,
    const JavaParamRef<jobject>& j_depths_obj) {
  DCHECK(IsLoaded());

  std::unique_ptr<icu::Collator> collator = GetICUCollator();

  // Vector to temporarily contain all child notes at same level for sorting
  std::vector<const Notes_Node*> noteList;

  // Stack for Depth-First Search of note model. It stores nodes and their
  // heights.
  base::stack<std::pair<const Notes_Node*, int>> stk;

  noteList.push_back(notes_model_->main_node());

  // Push all sorted top folders in stack and give them depth of 0.
  // Note the order to push folders to stack should be opposite to the order in
  // output.
  for (std::vector<const Notes_Node*>::reverse_iterator it =
           noteList.rbegin();
       it != noteList.rend();
       ++it) {
    stk.push(std::make_pair(*it, 0));
  }

  while (!stk.empty()) {
    const Notes_Node* node = stk.top().first;
    int depth = stk.top().second;
    stk.pop();
    Java_NotesBridge_addToNoteIdListWithDepth(env,
                                              j_folders_obj,
                                              node->id(),
                                              GetNoteType(node),
                                              j_depths_obj,
                                              depth);
    noteList.clear();
    for (const auto& child : node->children()) {
      if (child->is_folder()) {
        noteList.push_back(child.get());
      }
    }
    std::stable_sort(noteList.begin(),
                     noteList.end(),
                     NoteTitleComparer(this, collator.get()));
    for (std::vector<const Notes_Node*>::reverse_iterator it =
             noteList.rbegin();
         it != noteList.rend();
         ++it) {
      stk.push(std::make_pair(*it, depth + 1));
    }
  }
}

ScopedJavaLocalRef<jobject> NotesBridge::GetRootFolderId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  const Notes_Node* root_node = notes_model_->root_node();
  ScopedJavaLocalRef<jobject> folder_id_obj =
      JavaNoteIdCreateNoteId(
          env, 0 /*root_node->id()*/, GetNoteType(root_node));
  return folder_id_obj;
}

ScopedJavaLocalRef<jobject> NotesBridge::GetMainFolderId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  const Notes_Node* main_node = notes_model_->main_node();
  ScopedJavaLocalRef<jobject> folder_id_obj =
      JavaNoteIdCreateNoteId(
          env, main_node->id(), GetNoteType(main_node));
  return folder_id_obj;
}

ScopedJavaLocalRef<jobject> NotesBridge::GetTrashFolderId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  const Notes_Node* trash_node = notes_model_->trash_node();
  ScopedJavaLocalRef<jobject> folder_id_obj =
      JavaNoteIdCreateNoteId(
          env, trash_node->id(), GetNoteType(trash_node));
  return folder_id_obj;
}


ScopedJavaLocalRef<jobject> NotesBridge::GetOtherFolderId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  const Notes_Node* other_node = notes_model_->other_node();
  ScopedJavaLocalRef<jobject> folder_id_obj =
      JavaNoteIdCreateNoteId(
          env, other_node->id(), GetNoteType(other_node));
  return folder_id_obj;
}

jint NotesBridge::GetChildCount(JNIEnv* env,
                                const JavaParamRef<jobject>& obj,
                                jlong id,
                                jint type) {
  DCHECK(IsLoaded());
  const Notes_Node* node = GetNodeByID(id, type);
  return jint{node->children().size()};
}

void NotesBridge::GetChildIDs(JNIEnv* env,
                              const JavaParamRef<jobject>& obj,
                              jlong id,
                              jint type,
                              jboolean get_folders,
                              jboolean get_notes,
                              jboolean get_separators,
                              const JavaParamRef<jobject>& j_result_obj) {
  DCHECK(IsLoaded());

  const Notes_Node* parent = GetNodeByID(id, type);
  if (!parent->is_folder() || !IsReachable(parent))
    return;

  // Get the folder contents
  for (const auto& child : parent->children()) {

    if (IsFolderAvailable(child.get()) && IsReachable(child.get()) &&
        (child->is_folder() ? get_folders : (child->is_separator() ? get_separators : get_notes) )) {
      Java_NotesBridge_addToNoteIdList(env, j_result_obj, child->id(),
                                       GetNoteType(child.get()));
    }
  }
}

ScopedJavaLocalRef<jobject> NotesBridge::GetChildAt(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong id,
    jint type,
    jint index) {
  DCHECK(IsLoaded());

  const Notes_Node* parent = GetNodeByID(id, type);
  DCHECK(parent);
  const Notes_Node* child = parent->children()[size_t{index}].get();
  return JavaNoteIdCreateNoteId(env, child->id(), GetNoteType(child));
}

jint NotesBridge::GetTotalNoteCount(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jlong id,
    jint type) {
  DCHECK(IsLoaded());

  std::queue<const Notes_Node*> nodes;
  const Notes_Node* parent = GetNodeByID(id, type);
  DCHECK(parent->is_folder());

  int count = 0;
  nodes.push(parent);
  while (!nodes.empty()) {
    const Notes_Node* node = nodes.front();
    nodes.pop();

    for (const auto& child : node->children()) {
      if (child->is_folder())
        nodes.push(child.get());
      else
        ++count;
    }
  }

  return count;
}

void NotesBridge::SetNoteTitle(JNIEnv* env,
                               const JavaParamRef<jobject>& obj,
                               jlong id,
                               jint type,
                               const JavaParamRef<jstring>& j_title) {
  DCHECK(IsLoaded());
  const Notes_Node* note = GetNodeByID(id, type);
  const base::string16 title =
      base::android::ConvertJavaStringToUTF16(env, j_title);

    notes_model_->SetTitle(note, title);
}

void NotesBridge::SetNoteContent(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj,
                                 jlong id,
                                 jint type,
                                 const JavaParamRef<jstring>& j_title) {
  DCHECK(IsLoaded());
  const Notes_Node* note = GetNodeByID(id, type);
  const base::string16 title =
      base::android::ConvertJavaStringToUTF16(env, j_title);

    notes_model_->SetContent(note, title);
}



void NotesBridge::SetNoteUrl(JNIEnv* env,
                             const JavaParamRef<jobject>& obj,
                             jlong id,
                             jint type,
                             const JavaParamRef<jstring>& url) {
  DCHECK(IsLoaded());
  notes_model_->SetURL(
      GetNodeByID(id, type),
      GURL(base::android::ConvertJavaStringToUTF16(env, url)));
}

bool NotesBridge::DoesNoteExist(JNIEnv* env,
                                const JavaParamRef<jobject>& obj,
                                jlong id,
                                jint type) {
  DCHECK(IsLoaded());

  const Notes_Node* node = GetNodeByID(id, type);

  if (!node)
    return false;

  if (type == Notes_Node::Type::NOTE) {
    return true;
  }
  return false;
}

void NotesBridge::GetNotesForFolder(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_folder_id_obj,
    const JavaParamRef<jobject>& j_callback_obj,
    const JavaParamRef<jobject>& j_result_obj) {
  DCHECK(IsLoaded());
  long folder_id = JavaNoteIdGetId(env, j_folder_id_obj);
  int type = JavaNoteIdGetType(env, j_folder_id_obj);
  const Notes_Node* folder = GetFolderWithFallback(folder_id, type);

  if (!folder->is_folder() || !IsReachable(folder))
    return;

  // Recreate the java noteId object due to fallback.
  ScopedJavaLocalRef<jobject> folder_id_obj =
      JavaNoteIdCreateNoteId(
          env, folder->id(), GetNoteType(folder));

  // Get the folder contents.
  for (const auto& node : folder->children()) {
    if (IsFolderAvailable(node.get()))
      ExtractNotes_NodeInformation(node.get(), j_result_obj);
  }

  if (j_callback_obj) {
    Java_NotesCallback_onNotesAvailable(env, j_callback_obj,
                                        folder_id_obj, j_result_obj);
  }
}

jboolean NotesBridge::IsFolderVisible(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj,
                                      jlong id,
                                      jint type) {
  if (type == Notes_Node::Type::NOTE) {
    return true;
  }
  return false;
}

void NotesBridge::GetCurrentFolderHierarchy(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_folder_id_obj,
    const JavaParamRef<jobject>& j_callback_obj,
    const JavaParamRef<jobject>& j_result_obj) {
  DCHECK(IsLoaded());
  long folder_id = JavaNoteIdGetId(env, j_folder_id_obj);
  int type = JavaNoteIdGetType(env, j_folder_id_obj);
  const Notes_Node* folder = GetFolderWithFallback(folder_id, type);

  if (!folder->is_folder() || !IsReachable(folder))
    return;

  // Recreate the java noteId object due to fallback.
  ScopedJavaLocalRef<jobject> folder_id_obj =
      JavaNoteIdCreateNoteId(
          env, folder->id(), GetNoteType(folder));

  // Get the folder hierarchy.
  const Notes_Node* node = folder;
  while (node) {
    ExtractNotes_NodeInformation(node, j_result_obj);
    node = GetParentNode(node);
  }

  Java_NotesCallback_onNotesFolderHierarchyAvailable(
      env, j_callback_obj, folder_id_obj, j_result_obj);
}

void NotesBridge::SearchNotes(JNIEnv* env,
                              const JavaParamRef<jobject>& obj,
                              const JavaParamRef<jobject>& j_list,
                              const JavaParamRef<jstring>& j_query,
                              jint max_results) {
  DCHECK(notes_model_->loaded());
  std::vector<std::pair<int, Notes_Node::Type>> results;
  notes_model_->GetNotesMatching(
  base::android::ConvertJavaStringToUTF16(env, j_query),
          max_results,
          &results);
  for (const std::pair<int, Notes_Node::Type>& node : results) {
    Java_NotesBridge_addToNoteIdList(env, j_list, node.first,
                                           node.second);
  }
}

ScopedJavaLocalRef<jobject> NotesBridge::AddFolder(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_parent_id_obj,
    jint index,
    const JavaParamRef<jstring>& j_title) {
  DCHECK(IsLoaded());
  long note_id = JavaNoteIdGetId(env, j_parent_id_obj);
  int type = JavaNoteIdGetType(env, j_parent_id_obj);
  const Notes_Node* parent = GetNodeByID(note_id, type);

  const Notes_Node* new_node = notes_model_->AddFolder(
      parent, index, base::android::ConvertJavaStringToUTF16(env, j_title));
  if (!new_node) {
    NOTREACHED();
    return ScopedJavaLocalRef<jobject>();
  }
  ScopedJavaLocalRef<jobject> new_java_obj =
      JavaNoteIdCreateNoteId(
          env, new_node->id(), GetNoteType(new_node));
  return new_java_obj;
}

void NotesBridge::DeleteNote(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_note_id_obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());

  long note_id = JavaNoteIdGetId(env, j_note_id_obj);
  int type = JavaNoteIdGetType(env, j_note_id_obj);
  const Notes_Node* node = GetNodeByID(note_id, type);
  if (!IsEditable(node)) {
    NOTREACHED();
    return;
  }
  notes_model_->Remove(node);
}

void NotesBridge::RemoveAllUserNotes(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());
  notes_model_->RemoveAllUserNotes();
}

void NotesBridge::MoveNote(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_note_id_obj,
    const JavaParamRef<jobject>& j_parent_id_obj,
    jint index) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());

  long note_id = JavaNoteIdGetId(env, j_note_id_obj);
  int type = JavaNoteIdGetType(env, j_note_id_obj);
  const Notes_Node* node = GetNodeByID(note_id, type);
  if (!IsEditable(node)) {
    NOTREACHED();
    return;
  }
  note_id = JavaNoteIdGetId(env, j_parent_id_obj);
  type = JavaNoteIdGetType(env, j_parent_id_obj);
  const Notes_Node* new_parent_node = GetNodeByID(note_id, type);
  notes_model_->Move(node, new_parent_node, index);
}

ScopedJavaLocalRef<jobject> NotesBridge::AddNote(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_parent_id_obj,
    jint index,
    const JavaParamRef<jstring>& j_content,
    const JavaParamRef<jstring>& j_url) {
  DCHECK(IsLoaded());
  long note_id = JavaNoteIdGetId(env, j_parent_id_obj);
  int type = JavaNoteIdGetType(env, j_parent_id_obj);
  const Notes_Node* parent = GetNodeByID(note_id, type);

  const Notes_Node* new_node = notes_model_->AddNote(
        parent,
        index,
        base::android::ConvertJavaStringToUTF16(env, j_content),
        GURL(base::android::ConvertJavaStringToUTF16(env, j_url)),
        base::android::ConvertJavaStringToUTF16(env, j_content));

  if (!new_node) {
    NOTREACHED();
    return ScopedJavaLocalRef<jobject>();
  }
  ScopedJavaLocalRef<jobject> new_java_obj =
      JavaNoteIdCreateNoteId(
          env, new_node->id(), GetNoteType(new_node));
  return new_java_obj;
}

// TODO: Undo not yet implemented
void NotesBridge::Undo(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());
 /* NoteUndoService* undo_service =
      NoteUndoServiceFactory::GetForProfile(profile_);
  UndoManager* undo_manager = undo_service->undo_manager();
  undo_manager->Undo();*/ // TODO
}

void NotesBridge::StartGroupingUndos(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());
 // DCHECK(!grouped_note_actions_.get()); // shouldn't have started already
 // grouped_note_actions_.reset(
  //    new notes::ScopedGroupNoteActions(notes_model_));
}

void NotesBridge::EndGroupingUndos(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());
  //DCHECK(grouped_note_actions_.get()); // should only call after start
  //grouped_note_actions_.reset();
}

base::string16 NotesBridge::GetContent(const Notes_Node* node) const {
  return node->GetContent();
}

base::string16 NotesBridge::GetTitle(const Notes_Node* node) const {
  return node->GetTitle();
}

ScopedJavaLocalRef<jobject> NotesBridge::CreateJavaNote(
    const Notes_Node* node) {
  JNIEnv* env = AttachCurrentThread();

  const Notes_Node* parent = GetParentNode(node);
  int64_t parent_id = parent ? parent->id() : -1;

  std::string url;
  //if (node->is_url())
    url = node->GetURL().spec();

  int64_t java_timestamp = node->GetCreationTime().ToJavaTime();

  return Java_NotesBridge_createNoteItem(
      env, node->id(), GetNoteType(node),
      ConvertUTF16ToJavaString(env, GetTitle(node)),
      ConvertUTF16ToJavaString(env, GetContent(node)),
      java_timestamp,
      ConvertUTF8ToJavaString(env, url), node->is_folder(), parent_id,
      /*parent == nullptr ? 0 : */GetNoteType(parent), IsEditable(node), IsManaged(node));
}

void NotesBridge::ExtractNotes_NodeInformation(
    const Notes_Node* node,
    const JavaRef<jobject>& j_result_obj) {
  JNIEnv* env = AttachCurrentThread();
  if (!IsReachable(node))
    return;
  Java_NotesBridge_addToList(env, j_result_obj, CreateJavaNote(node));
}

const Notes_Node* NotesBridge::GetNodeByID(long node_id, int type) {
  const Notes_Node* node;

  node = vivaldi::GetNotesNodeByID(notes_model_,
                                          static_cast<int64_t>(node_id));

  return node;
}

const Notes_Node* NotesBridge::GetFolderWithFallback(long folder_id,
                                                     int type) {
  const Notes_Node* folder = GetNodeByID(folder_id, type);
  if (!folder ||
      !IsFolderAvailable(folder)) {
      folder = notes_model_->root_node();
  }
  return folder;
}

bool NotesBridge::IsEditNotesEnabled() const {
  return true;
}

void NotesBridge::EditNotesEnabledChanged() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_NotesBridge_editNotesEnabledChanged(env, obj);
}

bool NotesBridge::IsEditable(const Notes_Node* node) const {
  if (!node || (node->type() != Notes_Node::FOLDER &&
      node->type() != Notes_Node::NOTE && node->type() != Notes_Node::SEPARATOR)) {
    return false;
  }
  if (!IsEditNotesEnabled() || notes_model_->is_permanent_node(node))
    return false;
  return true;
}

bool NotesBridge::IsManaged(const Notes_Node* node) const {
    return false;
}

const Notes_Node* NotesBridge::GetParentNode(const Notes_Node* node) {
  DCHECK(IsLoaded());
  return node->parent();
}

int NotesBridge::GetNoteType(const Notes_Node* node) {
  return notes::NoteType::NOTE_TYPE_NORMAL;//node->type();
}

bool NotesBridge::IsReachable(const Notes_Node* node) const {
  return true;
}

bool NotesBridge::IsLoaded() const {
  return (notes_model_->loaded());
}

bool NotesBridge::IsFolderAvailable(
    const Notes_Node* folder) const {
  auto* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_->GetOriginalProfile());
  return (folder->type() != Notes_Node::TRASH && // TODO
          folder->type() != Notes_Node::OTHER) ||
         (identity_manager && identity_manager->HasPrimaryAccount());
}

void NotesBridge::NotifyIfDoneLoading() {
  if (!IsLoaded())
    return;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_NotesBridge_noteModelLoaded(env, obj);
}

// ------------- Observer-related methods ------------- //

void NotesBridge::NotesModelChanged() {
  if (!IsLoaded())
    return;

  // Called when there are changes to the note model. It is most
  // likely changes to the partner notes.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_NotesBridge_noteModelChanged(env, obj);
}

void NotesBridge::NotesModelLoaded(vivaldi::Notes_Model* model,
                                          bool ids_reassigned) {
  NotifyIfDoneLoading();
}

void NotesBridge::NotesModelBeingDeleted(vivaldi::Notes_Model* model) {
  if (!IsLoaded())
    return;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_NotesBridge_noteModelDeleted(env, obj);
}

void NotesBridge::Notes_NodeMoved(vivaldi::Notes_Model* model,
                                  const Notes_Node* old_parent,
                                  size_t old_index,
                                  const Notes_Node* new_parent,
                                  size_t new_index) {
  if (!IsLoaded())
    return;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_NotesBridge_noteNodeMoved(
      env, obj, CreateJavaNote(old_parent), old_index,
      CreateJavaNote(new_parent), new_index);
}

void NotesBridge::Notes_NodeAdded(vivaldi::Notes_Model* model,
                                  const Notes_Node* parent,
                                  size_t index) {
  if (!IsLoaded())
    return;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_NotesBridge_noteNodeAdded(env, obj, CreateJavaNote(parent),
                                        index);
}

void NotesBridge::Notes_NodeRemoved(vivaldi::Notes_Model* model,
                                    const Notes_Node* parent,
                                    size_t old_index,
                                    const Notes_Node* node,
                                    const std::set<GURL>& removed_urls) {
  if (!IsLoaded())
    return;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_NotesBridge_noteNodeRemoved(env, obj, CreateJavaNote(parent),
                                   old_index, CreateJavaNote(node));
}

void NotesBridge::NoteAllUserNodesRemoved(
    vivaldi::Notes_Model* model,
    const std::set<GURL>& removed_urls) {
  if (!IsLoaded())
    return;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_NotesBridge_noteAllUserNodesRemoved(env, obj);
}

void NotesBridge::Notes_NodeChanged(vivaldi::Notes_Model* model,
                                    const Notes_Node* node) {
  if (!IsLoaded())
    return;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_NotesBridge_noteNodeChanged(env, obj, CreateJavaNote(node));
}

void NotesBridge::Notes_NodeChildrenReordered(vivaldi::Notes_Model* model,
                                              const Notes_Node* node) {
  if (!IsLoaded())
    return;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_NotesBridge_noteNodeChildrenReordered(env, obj,
                                             CreateJavaNote(node));
}

void NotesBridge::ExtensiveNoteChangesBeginning(vivaldi::Notes_Model* model) {
  if (!IsLoaded())
    return;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_NotesBridge_extensiveNoteChangesBeginning(env, obj);
}

void NotesBridge::ExtensiveNoteChangesEnded(vivaldi::Notes_Model* model) {
  if (!IsLoaded())
    return;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_NotesBridge_extensiveNoteChangesEnded(env, obj);
}

// Invoked when a node has been added.
void NotesBridge::NotesNodeAdded(vivaldi::Notes_Model* model,
                                 const vivaldi::Notes_Node* parent,
                                 size_t index) {
  Notes_NodeAdded(model, parent, index);
}

void NotesBridge::NotesNodeMoved(vivaldi::Notes_Model* model,
                                const vivaldi::Notes_Node* old_parent,
                                size_t old_index,
                                const vivaldi::Notes_Node* new_parent,
                                size_t new_index) {
  Notes_NodeMoved(model, old_parent, old_index, new_parent, new_index);
}

// Invoked when the title or url of a node changes.
void NotesBridge::NotesNodeChanged(vivaldi::Notes_Model* model, const vivaldi::Notes_Node* node) {
  Notes_NodeChanged(model, node);
}

void NotesBridge::NotesNodeRemoved(vivaldi::Notes_Model* model,
                                  const vivaldi::Notes_Node* parent,
                                  size_t old_index,
                                  const vivaldi::Notes_Node* node) {
  std::set<GURL> urls = {};
  Notes_NodeRemoved(model, parent, old_index, node, urls);
}
