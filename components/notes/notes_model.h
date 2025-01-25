// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_NOTES_NOTES_MODEL_H_
#define COMPONENTS_NOTES_NOTES_MODEL_H_

#include <memory>
#include <set>
#include <vector>

#include "base/location.h"
#include "base/uuid.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/notes/note_model_loader.h"
#include "components/notes/note_node.h"
#include "components/notes/notes_model_observer.h"
#include "importer/imported_notes_entry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
}

namespace sync_notes {
class NoteSyncService;
}

namespace file_sync {
class SyncedFileStore;
}

namespace vivaldi {

class NoteLoadDetails;
class NotesStorage;

class NotesModel : public KeyedService {
 public:
  struct URLAndTitle {
    GURL url;
    std::u16string title;
    std::u16string content;
  };

 public:
  explicit NotesModel(sync_notes::NoteSyncService* sync_service,
                      file_sync::SyncedFileStore* synced_file_store);
  ~NotesModel() override;
  NotesModel(const NotesModel&) = delete;
  NotesModel& operator=(const NotesModel&) = delete;

  // Triggers the loading of notes, which is an asynchronous operation with
  // most heavy-lifting taking place in a background sequence. Upon completion,
  // loaded() will return true and observers will be notified via
  // NotesModelLoaded().
  void Load(const base::FilePath& profile_path);

  void AddObserver(NotesModelObserver* observer);
  void RemoveObserver(NotesModelObserver* observer);

  // Notifies the observers that an extensive set of changes is about to happen,
  // such as during import or sync, so they can delay any expensive UI updates
  // until it's finished.
  void BeginExtensiveChanges();
  void EndExtensiveChanges();

  // Returns true if this notes model is currently in a mode where extensive
  // changes might happen, such as for import and sync. This is helpful for
  // observers that are created after the mode has started, and want to check
  // state during their own initializer, such as the NTP.
  bool IsDoingExtensiveChanges() const { return extensive_changes_ > 0; }

  // The root node, parent of the main node, trash and other nodes
  const NoteNode* root_node() const { return root_.get(); }

  // The parent node of all normal notes (deleted  notes are parented by the
  // trash node). Child of the root node
  const NoteNode* main_node() const { return main_node_; }

  // Returns the 'other' node. This is NULL until loaded. Child of the root node
  const NoteNode* other_node() const { return other_node_; }

  // Returns the trash node. This is NULL until loaded. Child of the root node
  const NoteNode* trash_node() const { return trash_node_; }

  // Returns whether the given |node| is one of the permanent nodes - root node,
  bool is_permanent_node(const NoteNode* node) const {
    return node && (node == root_.get() || node->parent() == root_.get());
  }

  // Adds a new folder node at the specified position with the given
  // |creation_time|, |last_modification_time|, and |uuid|. If no UUID is
  // provided (i.e. nullopt), then a random one will be generated. If a UUID is
  // provided, it must be valid.
  const NoteNode* AddFolder(
      const NoteNode* parent,
      size_t index,
      const std::u16string& name,
      std::optional<base::Time> creation_time = std::nullopt,
      std::optional<base::Time> last_modification_time = std::nullopt,
      std::optional<base::Uuid> uuid = std::nullopt);

  // Adds a note at the specified position with the given |creation_time|,
  // |last_modification_time|, and |uuid|. If no UUID is provided (i.e.
  // nullopt), then a random one will be generated. If a UUID is provided, it
  // must be valid.
  const NoteNode* AddNote(
      const NoteNode* parent,
      size_t index,
      const std::u16string& title,
      const GURL& url,
      const std::u16string& content,
      std::optional<base::Time> creation_time = std::nullopt,
      std::optional<base::Time> last_modification_time = std::nullopt,
      std::optional<base::Uuid> uuid = std::nullopt);

  // Adds a separator at the specified position with the given |creation_time|
  // and |uuid|. If no UUID is provided (i.e. nullopt), then a random one will
  // be generated. If a UUID is provided, it must be valid.
  const NoteNode* AddSeparator(
      const NoteNode* parent,
      size_t index,
      std::optional<std::u16string> name = std::nullopt,
      std::optional<base::Time> creation_time = std::nullopt,
      std::optional<base::Uuid> uuid = std::nullopt);

  // Adds an attachment for which only the |checksum| is known at the specified
  // position with the given |creation_time| and |uuid|. If no UUID is provided
  // (i.e. nullopt), then a random one will be generated. If a UUID is provided,
  // it must be valid.
  const NoteNode* AddAttachmentFromChecksum(
      const NoteNode* parent,
      size_t index,
      const std::u16string& title,
      const GURL& url,
      const std::string& checksum,
      std::optional<base::Time> creation_time = std::nullopt,
      std::optional<base::Uuid> uuid = std::nullopt);

  // Adds an attachment at the specified position with the given |creation_time|
  // and |uuid|. If no UUID is provided (i.e. nullopt), then a random one will
  // be generated. If a UUID is provided, it must be valid.
  const NoteNode* AddAttachment(
      const NoteNode* parent,
      size_t index,
      const std::u16string& title,
      const GURL& url,
      std::vector<uint8_t> content,
      std::optional<base::Time> creation_time = std::nullopt,
      std::optional<base::Uuid> uuid = std::nullopt);

  const NoteNode* ImportNote(const NoteNode* parent,
                             size_t index,
                             const ImportedNotesEntry& node);

  // Removes the node at the given |index| from |parent|. Removing a folder node
  // recursively removes all nodes.
  void Remove(const NoteNode* node, const base::Location& location);

  // Removes all the non-permanent notes nodes that are editable by the user.
  // Observers are only notified when all nodes have been removed. There is no
  // notification for individual node removals.
  void RemoveAllUserNotes(const base::Location& location);

  // Moves |node| to |new_parent| and inserts it at the given |index|.
  void Move(const NoteNode* node, const NoteNode* new_parent, size_t index);

  void SetURL(const NoteNode* node,
              const GURL& url,
              bool updateLastModificationTime = true);
  void SetTitle(const NoteNode* node,
                const std::u16string& title,
                bool updateLastModificationTime = true);

  // Sets the date added time of |node|.
  void SetDateAdded(const NoteNode* node, base::Time date_added);
  void SetLastModificationTime(const NoteNode* node,
                               const base::Time time = base::Time::Now());

  void SetContent(const NoteNode* node,
                  const std::u16string& content,
                  bool updateLastModificationTime = true);

  // Returns, by reference in |notes|, the set of notes urls and their titles
  // and content. This returns the unique set of URLs. For example, if two notes
  // reference the same URL only one entry is added not matter the titles are
  // same or not.
  //
  // If not on the main thread you *must* invoke BlockTillLoaded first.
  // NOTE: This is a function only used by unit tests
  void GetNotes(std::vector<NotesModel::URLAndTitle>* urls);

  bool loaded() const { return loaded_; }

  // Returns true if the parent and index are valid.
  bool IsValidIndex(const NoteNode* parent, size_t index, bool allow_end);

  bool is_root_node(const NoteNode* node) const { return node == root_.get(); }
  bool is_main_node(const NoteNode* node) const { return node == main_node_; }
  bool is_other_node(const NoteNode* node) const { return node == other_node_; }

  // Notifies the observers that a set of changes initiated by a single user
  // action is about to happen and has completed.
  void BeginGroupedChanges();
  void EndGroupedChanges();

  // Generates and returns the next node ID.
  int64_t generate_next_node_id();

  // Sorts the children of |parent|, notifying observers by way of the
  // NotesNodeChildrenReordered method.
  void SortChildren(const NoteNode* parent);

  // Order the children of |parent| as specified in |ordered_nodes|.  This
  // function should only be used to reorder the child nodes of |parent| and
  // is not meant to move nodes between different parent. Notifies observers
  // using the NotesNodeChildrenReordered method.
  void ReorderChildren(const NoteNode* parent,
                       const std::vector<const NoteNode*>& ordered_nodes);

  // Implementation of IsNotes. Before calling this the caller must obtain
  // a lock on |url_lock_|.
  bool IsNotesNoLock(const GURL& url);

  // Returns the set of nodes with the |url|.
  void GetNodesByURL(const GURL& url, std::vector<const NoteNode*>* nodes);

  void set_next_node_id(int64_t id) { next_node_id_ = id; }

  void GetNotesMatching(const std::u16string& text,
                        size_t max_count,
                        std::vector<const NoteNode*>& matches);

  void GetNotesFoldersMatching(const std::u16string& text,
                               size_t max_count,
                               std::vector<const NoteNode*>& matches);

  const NoteNode* GetNoteNodeByID(int64_t id) const;

  sync_notes::NoteSyncService* sync_service() { return sync_service_; }

  base::WeakPtr<NotesModel> AsWeakPtr() { return weak_factory_.GetWeakPtr(); }

 private:
  friend class NotesCodecTest;
  friend std::unique_ptr<NotesModel> CreateTestNotesModel();

  std::unique_ptr<NoteLoadDetails> CreateLoadDetails();
  NoteNode* GetOrCreateTrashNode();

  // Used to order NoteNodes by URL.
  class NodeURLComparator {
   public:
    bool operator()(const NoteNode* n1, const NoteNode* n2) const {
      return n1->GetURL() < n2->GetURL();
    }
  };

  // Remove |node| from |nodes_ordered_by_url_set_|.
  void RemoveNodeFromURLSet(NoteNode* node);
  // Remove |node| and all its children from |nodes_ordered_by_url_set_|.
  void RemoveNodeTreeFromURLSet(NoteNode* node);

  void RemoveAttachmentsRecursive(NoteNode* node);

  void DoneLoading(std::unique_ptr<NoteLoadDetails> details);
  void OnSyncedFilesStoreLoaded(std::unique_ptr<NoteLoadDetails> details);
  void MigrateAttachmentsRecursive(NoteNode* node);

  NoteNode* AddNode(NoteNode* parent,
                    int index,
                    std::unique_ptr<NoteNode> node);

  // Update |node| lastModificationTime to base::Time::Now()
  void UpdateLastModificationTime(const NoteNode* node);

  std::unique_ptr<NoteNode> root_;

  raw_ptr<PermanentNoteNode> main_node_ = nullptr;
  raw_ptr<PermanentNoteNode> other_node_ = nullptr;
  raw_ptr<PermanentNoteNode> trash_node_ = nullptr;

  bool loaded_ = false;

  // The observers.
  base::ObserverList<NotesModelObserver> observers_;

  // Set of nodes ordered by URL. This is not a map to avoid copying the
  // urls.
  // WARNING: |nodes_ordered_by_url_set_| is accessed on multiple threads. As
  // such, be sure and wrap all usage of it around |url_lock_|.
  typedef std::multiset<NoteNode*, NodeURLComparator> NodesOrderedByURLSet;
  NodesOrderedByURLSet nodes_ordered_by_url_set_;
  base::Lock url_lock_;

  // See description of IsDoingExtensiveChanges above.
  int extensive_changes_ = 0;

  // Writes notes to disk.
  std::unique_ptr<NotesStorage> store_;

  // current id for nodes. Used in getNewIndex()
  int64_t next_node_id_ = 1;

  const raw_ptr<sync_notes::NoteSyncService> sync_service_;
  const raw_ptr<file_sync::SyncedFileStore> synced_file_store_;

  base::WeakPtrFactory<NotesModel> weak_factory_{this};
};

}  // namespace vivaldi

#endif  // COMPONENTS_NOTES_NOTES_MODEL_H_
