// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "notes/note_id.h"

#include "chrome/android/chrome_jni_headers/NoteId_jni.h"

namespace notes {
namespace android {

long JavaNoteIdGetId(JNIEnv* env, const base::android::JavaRef<jobject>& obj) {
  return Java_NoteId_getId(env, obj);
}

int JavaNoteIdGetType(JNIEnv* env, const base::android::JavaRef<jobject>& obj) {
  return Java_NoteId_getType(env, obj);
}

base::android::ScopedJavaLocalRef<jobject> JavaNoteIdCreateNoteId(JNIEnv* env,
                                                                  jlong id,
                                                                  jint type) {
  return Java_NoteId_createNoteId(env, id, type);
}

}  // namespace android
}  // namespace notes
