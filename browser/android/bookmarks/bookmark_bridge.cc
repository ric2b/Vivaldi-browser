// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "chrome/browser/android/bookmarks/bookmark_bridge.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/bookmarks/browser/bookmark_model.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using bookmarks::BookmarkNode;

void BookmarkBridge::SetBookmarkDescription(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj,
                                            jlong id,
                                            jint type,
                                            const JavaParamRef<jstring>&
                                                j_description) {
  DCHECK(IsLoaded());
  const BookmarkNode* bookmark = GetNodeByID(id, type);
  const base::string16 description =
      base::android::ConvertJavaStringToUTF16(env, j_description);
  bookmark_model_->SetDescription(bookmark, description);
}

void BookmarkBridge::SetBookmarkNickName(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj,
                                         jlong id,
                                         jint type,
                                         const JavaParamRef<jstring>& j_nick) {
  DCHECK(IsLoaded());
  const BookmarkNode* bookmark = GetNodeByID(id, type);
  const base::string16 nick =
      base::android::ConvertJavaStringToUTF16(env, j_nick);
  bookmark_model_->SetNickName(bookmark, nick);
}

void BookmarkBridge::SetBookmarkSpeedDial(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj,
                                          jlong id,
                                          jint type,
                                          jboolean is_speeddial) {
  DCHECK(IsLoaded());
  const BookmarkNode* bookmark = GetNodeByID(id, type);
  bookmark_model_->SetFolderAsSpeedDial(bookmark, is_speeddial);
}

base::string16 BookmarkBridge::GetNickName(const BookmarkNode* node) const {
    return node->GetNickName();
}

base::string16 BookmarkBridge::GetFaviconUrl(const BookmarkNode* node) const {
    return node->GetDefaultFaviconUri();
}

base::string16 BookmarkBridge::GetThumbnail(const BookmarkNode* node) const {
    return node->GetThumbnail();
}
