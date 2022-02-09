// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "chrome/browser/importer/profile_writer.h"

#include "app/vivaldi_resources.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/vivaldi_bookmark_kit.h"
#include "importer/imported_notes_entry.h"
#include "importer/imported_speeddial_entry.h"
#include "notes/note_node.h"
#include "notes/notes_factory.h"
#include "notes/notes_model.h"
#include "ui/base/l10n/l10n_util.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using vivaldi::NoteNode;
using vivaldi::NotesModel;
using vivaldi::NotesModelFactory;

namespace {

// Generates a unique folder name. If |folder_name| is not unique, then this
// repeatedly tests for '|folder_name| + (i)' until a unique name is found.
std::u16string VivaldiGenerateUniqueFolderName(
    BookmarkModel* model,
    const std::u16string& folder_name) {
  // Build a set containing the bookmark bar folder names.
  std::set<std::u16string> existing_folder_names;
  const BookmarkNode* bookmark_bar = model->bookmark_bar_node();
  for (const auto& node : bookmark_bar->children()) {
    if (node->is_folder())
      existing_folder_names.insert(node->GetTitle());
  }

  // If the given name is unique, use it.
  if (existing_folder_names.find(folder_name) == existing_folder_names.end())
    return folder_name;

  // Otherwise iterate until we find a unique name.
  for (size_t i = 1; i <= existing_folder_names.size(); ++i) {
    std::u16string name =
        folder_name + u" (" + base::NumberToString16(i) + u")";
    if (existing_folder_names.find(name) == existing_folder_names.end())
      return name;
  }

  NOTREACHED();
  return folder_name;
}

}  // namespace

void ProfileWriter::AddSpeedDial(
    const std::vector<ImportedSpeedDialEntry>& speeddial) {
  if (speeddial.empty())
    return;

  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile_);
  DCHECK(model->loaded());
  const BookmarkNode* bookmark_bar = model->bookmark_bar_node();
  const std::u16string& first_folder_name =
      l10n_util::GetStringUTF16(IDS_SPEEDDIAL_GROUP_FROM_OPERA);

  model->BeginExtensiveChanges();

  const BookmarkNode* top_level_folder = NULL;

  std::u16string name =
      VivaldiGenerateUniqueFolderName(model, first_folder_name);

  vivaldi_bookmark_kit::CustomMetaInfo vivaldi_meta;
  vivaldi_meta.SetSpeeddial(true);
  top_level_folder = model->AddFolder(
      bookmark_bar, bookmark_bar->children().size(), name, vivaldi_meta.map());

  for (auto& item : speeddial) {
    if (!model->AddURL(top_level_folder, top_level_folder->children().size(),
                       item.title, item.url))
      break;
  }

  model->EndExtensiveChanges();
}

void ProfileWriter::AddNotes(const std::vector<ImportedNotesEntry>& notes,
                             const std::u16string& top_level_folder_name) {
  NotesModel* model = NotesModelFactory::GetForBrowserContext(profile_);

  model->BeginExtensiveChanges();

  std::set<const NoteNode*> folders_added_to;
  const NoteNode* top_level_folder = NULL;
  for (std::vector<ImportedNotesEntry>::const_iterator note = notes.begin();
       note != notes.end(); ++note) {
    const NoteNode* parent = NULL;
    // Add to a folder that will contain all the imported notes.
    // The first time we do so, create the folder.
    if (!top_level_folder) {
      std::u16string name;
      name = l10n_util::GetStringUTF16(IDS_NOTES_GROUP_FROM_OPERA);
      top_level_folder = model->AddFolder(
          model->main_node(), model->main_node()->children().size(), name);
    }
    parent = top_level_folder;

    // Ensure any enclosing folders are present in the model.  The note's
    // enclosing folder structure should be
    //   path[0] > path[1] > ... > path[size() - 1]
    for (std::vector<std::u16string>::const_iterator folder_name =
             note->path.begin();
         folder_name != note->path.end(); ++folder_name) {
      const NoteNode* child = NULL;
      for (size_t index = 0; index < parent->children().size(); ++index) {
        const NoteNode* node = parent->children()[index].get();
        if (node->is_folder() && node->GetTitle() == *folder_name) {
          child = node;
          break;
        }
      }
      if (!child) {
        child =
            model->AddFolder(parent, parent->children().size(), *folder_name);
      }
      parent = child;
    }

    folders_added_to.insert(parent);
    model->ImportNote(parent, parent->children().size(), *note);
  }
  model->EndExtensiveChanges();
}