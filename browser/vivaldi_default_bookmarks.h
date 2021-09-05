// Copyright 2019 Vivaldi Technologies. All rights reserved.
//

#ifndef BROWSER_VIVALDI_DEFAULT_BOOKMARKS_H_
#define BROWSER_VIVALDI_DEFAULT_BOOKMARKS_H_

#include "base/callback.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"

class Profile;

namespace vivaldi_default_bookmarks {

extern bool g_bookmark_update_actve;

using UpdateCallback = base::OnceCallback<void(bool ok, bool no_version)>;

void UpdatePartners(Profile* profile,
                    const std::string& locale,
                    UpdateCallback callback = UpdateCallback());

}  // namespace vivaldi_default_bookmarks

#endif  // BROWSER_VIVALDI_DEFAULT_BOOKMARKS_H_
