#ifndef CHROME_BROWSER_ANDROID_NOTES_NOTES_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_NOTES_NOTES_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/compiler_specific.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/prefs/pref_change_registrar.h"

#include "notes/note_id.h"
#include "notes/note_node.h"
#include "notes/notes_factory.h"
#include "notes/notes_model.h"
#include "notes/notes_model_loaded_observer.h"
#include "notes/notes_model_observer.h"

// namespace notes {
// class ScopedGroupNoteActions;
//}

class Profile;

// The delegate to fetch notes information for the Android native
// notes page. This fetches the notes, title, urls, folder
// hierarchy.
class NotesBridge : public vivaldi::NotesModelObserver {
 public:
  NotesBridge(JNIEnv* env,
              const base::android::JavaRef<jobject>& obj,
              const base::android::JavaRef<jobject>& j_profile);
  void Destroy(JNIEnv*, const base::android::JavaParamRef<jobject>&);

  bool IsDoingExtensiveChanges(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);

  jboolean IsEditNotesEnabled(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jobject> GetNoteByID(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong id,
      jint type);

  void GetPermanentNodeIDs(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_result_obj);

  void GetTopLevelFolderParentIDs(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_result_obj);

  void GetTopLevelFolderIDs(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean get_special,
      jboolean get_normal,
      const base::android::JavaParamRef<jobject>& j_result_obj);

  void GetAllFoldersWithDepths(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_folders_obj,
      const base::android::JavaParamRef<jobject>& j_depths_obj);

  base::android::ScopedJavaLocalRef<jobject> GetRootFolderId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jobject> GetMainFolderId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jobject> GetTrashFolderId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jobject> GetOtherFolderId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jobject> GetDesktopFolderId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void GetChildIDs(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   jlong id,
                   jint type,
                   jboolean get_folders,
                   jboolean get_notes,
                   jboolean get_separators,
                   const base::android::JavaParamRef<jobject>& j_result_obj);

  jint GetChildCount(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     jlong id,
                     jint type);

  base::android::ScopedJavaLocalRef<jobject> GetChildAt(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong id,
      jint type,
      jint index);

  // Get the number of notes in the sub tree of the specified bookmark node.
  // The specified node must be of folder type.
  jint GetTotalNoteCount(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         jlong id,
                         jint type);

  void SetNoteTitle(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    jlong id,
                    jint type,
                    const base::android::JavaParamRef<jstring>& title);

  void SetNoteContent(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      jlong id,
                      jint type,
                      const base::android::JavaParamRef<jstring>& content);

  void SetNoteUrl(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& obj,
                  jlong id,
                  jint type,
                  const base::android::JavaParamRef<jstring>& url);

  bool DoesNoteExist(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     jlong id,
                     jint type);

  void GetNotesForFolder(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_folder_id_obj,
      const base::android::JavaParamRef<jobject>& j_callback_obj,
      const base::android::JavaParamRef<jobject>& j_result_obj);

  jboolean IsFolderVisible(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           jlong id,
                           jint type);

  void GetCurrentFolderHierarchy(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_folder_id_obj,
      const base::android::JavaParamRef<jobject>& j_callback_obj,
      const base::android::JavaParamRef<jobject>& j_result_obj);
  void SearchNotes(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   const base::android::JavaParamRef<jobject>& j_list,
                   const base::android::JavaParamRef<jstring>& j_query,
                   jint max_results);

  base::android::ScopedJavaLocalRef<jobject> AddFolder(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_parent_id_obj,
      jint index,
      const base::android::JavaParamRef<jstring>& j_title);

  void DeleteNote(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_bookmark_id_obj);

  void RemoveAllUserNotes(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);

  void MoveNote(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                const base::android::JavaParamRef<jobject>& j_bookmark_id_obj,
                const base::android::JavaParamRef<jobject>& j_parent_id_obj,
                jint index);

  base::android::ScopedJavaLocalRef<jobject> AddNote(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_parent_id_obj,
      jint index,
      const base::android::JavaParamRef<jstring>& j_title,
      const base::android::JavaParamRef<jstring>& j_url);

  void Undo(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  void StartGroupingUndos(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);

  void EndGroupingUndos(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);

  std::u16string GetContent(const vivaldi::NoteNode* node) const;
  std::u16string GetTitle(const vivaldi::NoteNode* node) const;

  // notes_model_observer
  // Invoked when the model has finished loading. |ids_reassigned| mirrors
  // TODO void NotesModelLoaded(vivaldi::NotesModel* model, bool ids_reassigned)
  // override {}

  // Invoked from the destructor of the NotesModel.
  // TODO void NotesModelBeingDeleted(vivaldi::NotesModel* model) override {}

  // Invoked when a node has moved.
  void NotesNodeMoved(vivaldi::NotesModel* model,
                      const vivaldi::NoteNode* old_parent,
                      size_t old_index,
                      const vivaldi::NoteNode* new_parent,
                      size_t new_index) override;

  // Invoked when a node has been added.
  void NotesNodeAdded(vivaldi::NotesModel* model,
                      const vivaldi::NoteNode* parent,
                      size_t index) override;

  /*   // Invoked before a node is removed.
     // |parent| the parent of the node that will be removed.
     // |old_index| the index of the node about to be removed in |parent|.
     // |node| is the node to be removed.
    void OnWillRemoveNotes(vivaldi::NotesModel* model,
                                    const vivaldi::NoteNode* parent,
                                    int old_index,
                                    const vivaldi::NoteNode* node) override {}*/

  // Invoked when a node has been removed, the item may still be starred though.
  // |parent| the parent of the node that was removed.
  // |old_index| the index of the removed node in |parent| before it was
  // removed.
  // |node| is the node that was removed.
  void NotesNodeRemoved(vivaldi::NotesModel* model,
                        const vivaldi::NoteNode* parent,
                        size_t old_index,
                        const vivaldi::NoteNode* node) override;

  /* // Invoked before the title or url of a node is changed.
   void OnWillChangeNotesNode(vivaldi::NotesModel* model,
                                      const vivaldi::NoteNode* node) override
   {}*/

  // Invoked when the title or url of a node changes.
  void NotesNodeChanged(vivaldi::NotesModel* model,
                        const vivaldi::NoteNode* node) override;

  // Invoked when a attachment has been loaded or changed.

  /*// Invoked before the direct children of |node| have been reordered in some
  // way, such as sorted.
  void OnWillReorderNotesNode(vivaldi::NotesModel* model,
                                      const vivaldi::NoteNode* node) override {}

  // Invoked when the children (just direct children, not descendants) of
  // |node| have been reordered in some way, such as sorted.
  void NotesNodeChildrenReordered(vivaldi::NotesModel* model,
                                          const vivaldi::NoteNode* node)
  override {}

  // Invoked before an extensive set of model changes is about to begin.
  // This tells UI intensive observers to wait until the updates finish to
  // update themselves.
  // These methods should only be used for imports and sync.
  // Observers should still respond to NotesNodeRemoved immediately,
  // to avoid holding onto stale node pointers.
  void ExtensiveNotesChangesBeginning(vivaldi::NotesModel* model) override {}

  // Invoked after an extensive set of model changes has ended.
  // This tells observers to update themselves if they were waiting for the
  // update to finish.
  void ExtensiveNotesChangesEnded(vivaldi::NotesModel* model) override {}

  // Invoked before all non-permanent notes nodes are removed.
  void OnWillRemoveAllNotes(vivaldi::NotesModel* model) override {}

  // Invoked when all non-permanent notes nodes have been removed.
  void NotesAllNodesRemoved(vivaldi::NotesModel* model) override {}

  void GroupedNotesChangesBeginning(vivaldi::NotesModel* model) override {}
  void GroupedNotesChangesEnded(vivaldi::NotesModel* model) override {}*/

  void ReorderChildren(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_note_id_obj,
      jlongArray arr);

 private:
  ~NotesBridge() override;
  NotesBridge(const NotesBridge&) = delete;
  NotesBridge& operator=(const NotesBridge&) = delete;

  base::android::ScopedJavaLocalRef<jobject> CreateJavaNote(
      const vivaldi::NoteNode* node);
  void ExtractNoteNodeInformation(
      const vivaldi::NoteNode* node,
      const base::android::JavaRef<jobject>& j_result_obj);
  const vivaldi::NoteNode* GetNodeByID(long node_id, int type);
  const vivaldi::NoteNode* GetFolderWithFallback(long folder_id, int type);
  bool IsEditNotesEnabled() const;
  void EditNotesEnabledChanged();
  // Returns whether |node| can be modified by the user.
  bool IsEditable(const vivaldi::NoteNode* node) const;
  // Returns whether |node| is a managed bookmark.
  bool IsManaged(const vivaldi::NoteNode* node) const;
  const vivaldi::NoteNode* GetParentNode(const vivaldi::NoteNode* node);
  int GetNoteType(const vivaldi::NoteNode* node);
  bool IsReachable(const vivaldi::NoteNode* node) const;
  bool IsLoaded() const;
  bool IsFolderAvailable(const vivaldi::NoteNode* folder) const;
  void NotifyIfDoneLoading();

  // Override vivaldi::BaseNotesModelObserver.
  // Called when there are changes to the notes model that don't trigger
  // any of the other callback methods. For example, this is called when
  // partner bookmarks change.
  void NotesModelChanged();
  void NotesModelLoaded(vivaldi::NotesModel* model,
                        bool ids_reassigned) override;
  void NotesModelBeingDeleted(vivaldi::NotesModel* model) override;
  void NoteNodeMoved(vivaldi::NotesModel* model,
                     const vivaldi::NoteNode* old_parent,
                     size_t old_index,
                     const vivaldi::NoteNode* new_parent,
                     size_t new_index);
  void NoteNodeAdded(vivaldi::NotesModel* model,
                     const vivaldi::NoteNode* parent,
                     size_t index);
  void NoteNodeRemoved(vivaldi::NotesModel* model,
                       const vivaldi::NoteNode* parent,
                       size_t old_index,
                       const vivaldi::NoteNode* node,
                       const std::set<GURL>& removed_urls);
  void NoteAllUserNodesRemoved(vivaldi::NotesModel* model,
                               const std::set<GURL>& removed_urls);
  void NoteNodeChanged(vivaldi::NotesModel* model,
                       const vivaldi::NoteNode* node);
  void NoteNodeChildrenReordered(vivaldi::NotesModel* model,
                                 const vivaldi::NoteNode* node);
  void ExtensiveNoteChangesBeginning(vivaldi::NotesModel* model);
  void ExtensiveNoteChangesEnded(vivaldi::NotesModel* model);

  const raw_ptr<Profile> profile_;
  const raw_ptr<vivaldi::NotesModel> notes_model_;
  JavaObjectWeakGlobalRef weak_java_ref_;
  PrefChangeRegistrar pref_change_registrar_;
};

#endif  // CHROME_BROWSER_ANDROID_NOTES_NOTES_BRIDGE_H_
