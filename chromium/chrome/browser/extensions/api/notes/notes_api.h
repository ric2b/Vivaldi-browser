// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_NOTES_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_NOTES_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/common/extensions/api/notes.h"
#include "notes/notesnode.h"
#include "notes/notes_model.h"

using Vivaldi::Notes_Model;
using Vivaldi::Notes_Node;
//using Vivaldi::Notes_Entry;
using Vivaldi::Notes_attachment;

namespace extensions {

using api::notes::NoteTreeNode;
using api::notes::NoteAttachment;

class NotesAsyncFunction : public ChromeAsyncExtensionFunction {
public:
  Notes_Node* GetNodeFromId(Notes_Node* node, int64 id);  // TODO: Move to superclass and subclass here.
protected:
  ~NotesAsyncFunction() override {}
};

class NotesGetFunction : public NotesAsyncFunction {
public:
  DECLARE_EXTENSION_FUNCTION("notes.get", NOTES_GET)
  NotesGetFunction();
protected:
  ~NotesGetFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;
};

class NotesGetChildrenFunction : public NotesAsyncFunction {
public:
  DECLARE_EXTENSION_FUNCTION("notes.getChildren", NOTES_GETCHILDREN)
  NotesGetChildrenFunction();
protected:
  ~NotesGetChildrenFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;
};

class NotesGetTreeFunction : public NotesAsyncFunction {
public:
  DECLARE_EXTENSION_FUNCTION("notes.getTree", NOTES_GETTREE)
  NotesGetTreeFunction();
protected:
  ~NotesGetTreeFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;
};

class NotesMoveFunction : public NotesAsyncFunction {
public:
  DECLARE_EXTENSION_FUNCTION("notes.move", NOTES_MOVE)
  NotesMoveFunction();
protected:
  ~NotesMoveFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;
};

class NotesGetSubTreeFunction : public NotesAsyncFunction {
public:
  DECLARE_EXTENSION_FUNCTION("notes.getSubTree", NOTES_GETSUBTREE)
  NotesGetSubTreeFunction();
protected:
  ~NotesGetSubTreeFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;
};

class NotesSearchFunction : public NotesAsyncFunction {
public:
  DECLARE_EXTENSION_FUNCTION("notes.search", NOTES_SEARCH)
  NotesSearchFunction();
protected:
  ~NotesSearchFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;
};

class NotesCreateFunction : public NotesAsyncFunction {
public:
  DECLARE_EXTENSION_FUNCTION("notes.create", NOTES_CREATE)
  NotesCreateFunction();
protected:
  ~NotesCreateFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;
};

class NotesUpdateFunction : public NotesAsyncFunction {
public:
  DECLARE_EXTENSION_FUNCTION("notes.update", NOTES_UPDATE)
  NotesUpdateFunction();
protected:
  ~NotesUpdateFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;
};

class NotesRemoveFunction : public NotesAsyncFunction {
public:
  DECLARE_EXTENSION_FUNCTION("notes.remove", NOTES_REMOVE)
  NotesRemoveFunction();
protected:
  ~NotesRemoveFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;
};

class NotesRemoveTreeFunction : public NotesAsyncFunction {
public:
  DECLARE_EXTENSION_FUNCTION("notes.removeTree", NOTES_REMOVETREE)
  NotesRemoveTreeFunction();
protected:
  ~NotesRemoveTreeFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_NOTES_API_H_

