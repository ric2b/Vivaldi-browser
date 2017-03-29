// Copyright (c) 2013-2014 Vivaldi Technologies AS. All rights reserved

#ifndef VIVALDI_NOTES_MODEL_H_
#define VIVALDI_NOTES_MODEL_H_

#include "notes/notesnode.h"
#include "notes/notes_model_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"

class Profile;

namespace base {
  class SequencedTaskRunner;
}

namespace Vivaldi {

class NotesLoadDetails;
class NotesStorage;

class Notes_Model : public content::NotificationObserver,
  public KeyedService
{
public:
  Notes_Model(Profile* profile);

  ~Notes_Model() override;

  // Loads the bookmarks. This is called upon creation of the
  // BookmarkModel. You need not invoke this directly.
  // All load operations will be executed on |task_runner|.
  void Load(const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  // Called from shutdown service before shutting down the browser
  void Shutdown() override;

  bool LoadNotes();
  bool SaveNotes();

  void AddObserver(NotesModelObserver* observer);
  void RemoveObserver(NotesModelObserver* observer);

  void Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) override;

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

  Notes_Node *root() { return &root_; }

  Notes_Node *AddFolder(const Notes_Node *parent, int index, const base::string16 &name);

  Notes_Node *AddNote(const Notes_Node *parent, int index, const base::string16 &subject, const GURL &url, const base::string16 &content);

  Notes_Node *AddNode(Notes_Node *parent, int index, Notes_Node *node);

  // Removes the node at the given |index| from |parent|. Removing a folder node
  // recursively removes all nodes.
  bool Remove(Notes_Node* parent, int index);

  // Moves |node| to |new_parent| and inserts it at the given |index|.
  bool Move(const Notes_Node* node,
    const Notes_Node* new_parent,
    int index);

  void DoneLoading(NotesLoadDetails* details);
  void BlockTillLoaded();

  bool loaded() const { return loaded_; }
  // Note that |root_| gets 0 as |id_|.
  int64 getNewIndex() { return ++current_index_; }

private:
  NotesLoadDetails* CreateLoadDetails();


private:
  Profile* profile_;
  Notes_Node root_;

  bool loaded_;

  base::WaitableEvent loaded_signal_;

  // The observers.
  base::ObserverList<NotesModelObserver> observers_;

  // See description of IsDoingExtensiveChanges above.
  int extensive_changes_;

  // Reads/writes bookmarks to disk.
  scoped_refptr<NotesStorage> store_;

  // current id for nodes. Used in getNewIndex()
  int64 current_index_;

};

} // namespace Vivaldi
#endif // VIVALDI_NOTES_MODEL_H_
