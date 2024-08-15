// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_NOTES_NOTE_ID_H_
#define COMPONENTS_NOTES_NOTE_ID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"

namespace notes {
namespace android {

// See NoteId#getId
long JavaNoteIdGetId(JNIEnv* env, const base::android::JavaRef<jobject>& obj);

// See NoteId#createNoteId
base::android::ScopedJavaLocalRef<jobject> JavaNoteIdCreateNoteId(JNIEnv* env,
                                                                  jlong id);

}  // namespace android
}  // namespace notes

#endif  // COMPONENTS_NOTES_NOTE_ID_H_
