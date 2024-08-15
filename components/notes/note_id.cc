// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "components/notes/note_id.h"

#include "chrome/android/chrome_jni_headers/NoteId_jni.h"

namespace notes {
namespace android {

long JavaNoteIdGetId(JNIEnv* env, const base::android::JavaRef<jobject>& obj) {
  return Java_NoteId_getId(env, obj);
}

base::android::ScopedJavaLocalRef<jobject> JavaNoteIdCreateNoteId(JNIEnv* env,
                                                                  jlong id) {
  return Java_NoteId_createNoteId(env, id);
}

}  // namespace android
}  // namespace notes
