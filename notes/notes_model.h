// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved

#ifndef NOTES_NOTES_MODEL_H_
#define NOTES_NOTES_MODEL_H_

#include <memory>
#include <set>
#include <vector>

#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "importer/imported_notes_entry.h"
#include "notes/notes_model_observer.h"
#include "notes/notesnode.h"

class Profile;

namespace base {
class SequencedTaskRunner;
}

namespace content {
class BrowserContext;
}

namespace vivaldi {

class NotesLoadDetails;
class NotesStorage;

class Notes_Model : public KeyedService {
 public:
  struct URLAndTitle {
    GURL url;
    base::string16 title;
    base::string16 content;
  };

 public:
  explicit Notes_Model(content::BrowserContext* context);

  ~Notes_Model() override;

  static std::unique_ptr<Notes_Model> CreateModel();

  // Loads the notes. This is called upon creation of the
  // Notes_Model. You need not invoke this directly.
  // All load operations will be executed on |task_runner|.
  void Load(const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  // Called from shutdown service before shutting down the browser
  void Shutdown() override;

  bool LoadNotes();
  bool SaveNotes();

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
  const Notes_Node* root_node() const { return &root_; }
  Notes_Node* root_node() { return &root_; }

  // Holds all deleted, but unflushed notes. Child of the root node
  Notes_Node* trash_node() { return trash_node_; }

  // The parent node of all normal notes (deleted  notes are parented by the
  // trash node). Child of the root node
  const Notes_Node* main_node() const { return main_node_; }
  Notes_Node* main_node() { return main_node_; }

  // Returns the 'other' node. This is NULL until loaded. Child of the root node
  const Notes_Node* other_node() const { return other_node_; }
  Notes_Node* other_node() { return other_node_; }

  // Returns whether the given |node| is one of the permanent nodes - root node,
  bool is_permanent_node(const Notes_Node* node) const {
    return node && (node == &root_ || node->parent() == &root_);
  }

  Notes_Node* AddFolder(const Notes_Node* parent,
                        int index,
                        const base::string16& name);

  Notes_Node* AddNote(const Notes_Node* parent,
                      int index,
                      const base::string16& subject,
                      const GURL& url,
                      const base::string16& content);

  Notes_Node* AddNode(Notes_Node* parent,
                      int index,
                      std::unique_ptr<Notes_Node> node);

  Notes_Node* AddNote(const Notes_Node* parent,
                      int index,
                      bool is_folder,
                      const ImportedNotesEntry& node);

  // Removes the node at the given |index| from |parent|. Removing a folder node
  // recursively removes all nodes.
  void Remove(const Notes_Node* node);
  bool Remove(Notes_Node* parent, int index);

  // Removes all the non-permanent notes nodes that are editable by the user.
  // Observers are only notified when all nodes have been removed. There is no
  // notification for individual node removals.
  void RemoveAllUserNotes();

  // Moves |node| to |new_parent| and inserts it at the given |index|.
  bool Move(const Notes_Node* node, const Notes_Node* new_parent, int index);

  void DoneLoading(std::unique_ptr<NotesLoadDetails> details);

  void SetURL(const Notes_Node* node, const GURL& url);
  void SetTitle(const Notes_Node* node, const base::string16& title);

  // Sets the date added time of |node|.
  void SetDateAdded(const Notes_Node* node, base::Time date_added);
  void SetDateFolderModified(const Notes_Node* parent, const base::Time time);

  void SetContent(const Notes_Node* node, const base::string16& content);

  void ClearAttachments(const Notes_Node* node);
  void AddAttachment(const Notes_Node* node, NoteAttachment&& attachment);

  // Returns, by reference in |notes|, the set of notes urls and their titles
  // and content. This returns the unique set of URLs. For example, if two notes
  // reference the same URL only one entry is added not matter the titles are
  // same or not.
  //
  // If not on the main thread you *must* invoke BlockTillLoaded first.
  // NOTE: This is a function only used by unit tests
  void GetNotes(std::vector<Notes_Model::URLAndTitle>* urls);

  void BlockTillLoaded();

  bool loaded() const { return loaded_; }
  // Note that |root_| gets 0 as |id_|.
  int64_t GetNewIndex() { return ++current_index_; }

  // Sets the sync transaction version of |node|.
  void SetNodeSyncTransactionVersion(const Notes_Node* node,
                                     int64_t sync_transaction_version);
  // Returns true if the parent and index are valid.
  bool IsValidIndex(const Notes_Node* parent, int index, bool allow_end);

  bool is_root_node(const Notes_Node* node) const { return node == &root_; }
  bool is_main_node(const Notes_Node* node) const { return node == main_node_; }
  bool is_other_node(const Notes_Node* node) const {
    return node == other_node_;
  }

  // Notifies the observers that a set of changes initiated by a single user
  // action is about to happen and has completed.
  void BeginGroupedChanges();
  void EndGroupedChanges();

  // Sorts the children of |parent|, notifying observers by way of the
  // NotesNodeChildrenReordered method.
  void SortChildren(const Notes_Node* parent);

  // Order the children of |parent| as specified in |ordered_nodes|.  This
  // function should only be used to reorder the child nodes of |parent| and
  // is not meant to move nodes between different parent. Notifies observers
  // using the NotesNodeChildrenReordered method.
  void ReorderChildren(const Notes_Node* parent,
                       const std::vector<const Notes_Node*>& ordered_nodes);

  // Implementation of IsNotes. Before calling this the caller must obtain
  // a lock on |url_lock_|.
  bool IsNotesNoLock(const GURL& url);

  // Removes the node from internal maps and recurses through all children. If
  // the node is a url, its url is added to removed_urls.
  //
  // Returns the set of nodes with the |url|.
  void GetNodesByURL(const GURL& url, std::vector<const Notes_Node*>* nodes);

  void set_next_index_id(int64_t next_index) { current_index_ = next_index; }

 private:
  std::unique_ptr<NotesLoadDetails> CreateLoadDetails();
  Notes_Node* GetOrCreateTrashNode();

  // Used to order Notes_Nodes by URL.
  class NodeURLComparator {
   public:
    bool operator()(const Notes_Node* n1, const Notes_Node* n2) const {
      return n1->GetURL() < n2->GetURL();
    }
  };

  // Remove |node| from |nodes_ordered_by_url_set_|.
  void RemoveNodeFromURLSet(Notes_Node* node);
  // Remove |node| and all its children from |nodes_ordered_by_url_set_|.
  void RemoveNodeTreeFromURLSet(Notes_Node* node);

  void RemoveAndDeleteNode(Notes_Node* parent, int index, Notes_Node* node_ptr);

  content::BrowserContext* context_;
  Notes_Node root_;
  Notes_Node* main_node_;
  Notes_Node* other_node_;

  // Points to the permanent trash node in the model.
  Notes_Node* trash_node_;

  bool loaded_;

  base::WaitableEvent loaded_signal_;

  // The observers.
  base::ObserverList<NotesModelObserver> observers_;

  // Set of nodes ordered by URL. This is not a map to avoid copying the
  // urls.
  // WARNING: |nodes_ordered_by_url_set_| is accessed on multiple threads. As
  // such, be sure and wrap all usage of it around |url_lock_|.
  typedef std::multiset<Notes_Node*, NodeURLComparator> NodesOrderedByURLSet;
  NodesOrderedByURLSet nodes_ordered_by_url_set_;
  base::Lock url_lock_;

  // See description of IsDoingExtensiveChanges above.
  int extensive_changes_;

  // Reads/writes notes to disk.
  std::unique_ptr<NotesStorage> store_;

  // current id for nodes. Used in getNewIndex()
  int64_t current_index_;

  DISALLOW_COPY_AND_ASSIGN(Notes_Model);
};

const Notes_Node* GetNotesNodeByID(const Notes_Model* model, int64_t id);
const Notes_Node* GetNodeByID(const Notes_Node* node, int64_t id);

Notes_Node* AsMutable(const Notes_Node* node);

}  // namespace vivaldi

#endif  // NOTES_NOTES_MODEL_H_
