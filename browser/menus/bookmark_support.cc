// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "browser/menus/bookmark_support.h"

#include "base/base64.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "ui/gfx/image/image.h"

#include "components/bookmarks/vivaldi_bookmark_kit.h"

namespace vivaldi {

BookmarkSupport::BookmarkSupport() : icons(kMax), observer_enabled(false) {}

BookmarkSupport::~BookmarkSupport() {}

void BookmarkSupport::initIcons(const std::vector<std::string>& src_icons) {
  if (src_icons.size() == kMax) {
    for (int i = 0; i < kMax; i++) {
      std::string png_data;
      if (base::Base64Decode(src_icons.at(i), &png_data)) {
        icons[i] = gfx::Image::CreateFrom1xPNGBytes(
            base::span(reinterpret_cast<const unsigned char*>(png_data.c_str()),
                       png_data.length()));
      }
    }
  }
}

const gfx::Image& BookmarkSupport::iconForNode(
    const bookmarks::BookmarkNode* node) const {
  if (node->is_folder()) {
    return vivaldi_bookmark_kit::GetSpeeddial(node) ? icons[kSpeeddial]
                                                    : icons[kFolder];
  }
  return icons[kUrl];
}

}  // namespace vivaldi