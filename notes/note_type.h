// Copyright (c) 2013-2015 Vivaldi Technologies AS. All rights reserved

#ifndef NOTES_NOTE_TYPE_H_
#define NOTES_NOTE_TYPE_H_

namespace notes {

enum NoteType {
  NOTE_TYPE_NORMAL,
  NOTE_TYPE_OTHER,
  // NOTE_TYPE_LAST must be the last element.
  NOTE_TYPE_LAST = NOTE_TYPE_OTHER,
};
}

#endif  // NOTES_NOTE_TYPE_H
