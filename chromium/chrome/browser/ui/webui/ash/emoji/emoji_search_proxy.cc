// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/emoji/emoji_search_proxy.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "chromeos/ash/components/emoji/emoji_search.h"
#include "chromeos/ash/components/emoji/emoji_search.mojom.h"

namespace ash {

namespace {

emoji_search::mojom::SearchResultsPtr SearchResultsFromEmojiSearchEntries(
    std::vector<emoji::EmojiSearchEntry> entries) {
  std::vector<std::string> strings;
  strings.reserve(entries.size());
  for (emoji::EmojiSearchEntry& entry : entries) {
    strings.push_back(std::move(entry).emoji_string);
  }
  return emoji_search::mojom::SearchResults::New(std::move(strings));
}

}  // namespace

EmojiSearchProxy::EmojiSearchProxy(
    mojo::PendingReceiver<emoji_search::mojom::EmojiSearch> receiver)
    : receiver_(this, std::move(receiver)) {
  // Before enabling flag - move search creation to background thread.
  search_ = std::make_unique<emoji::EmojiSearch>();
}

void EmojiSearchProxy::SetEmojiLanguage(const std::string& language_code,
                                        SetEmojiLanguageCallback callback) {
  CHECK(search_);
  std::move(callback).Run(search_->SetEmojiLanguage(language_code));
}

EmojiSearchProxy::~EmojiSearchProxy() {}

void EmojiSearchProxy::SearchEmoji(const std::string& query,
                                   SearchEmojiCallback callback) {
  CHECK(search_);
  emoji::EmojiSearchResult result = search_->SearchEmoji(query);
  std::move(callback).Run(
      SearchResultsFromEmojiSearchEntries(std::move(result.emojis)),
      SearchResultsFromEmojiSearchEntries(std::move(result.symbols)),
      SearchResultsFromEmojiSearchEntries(std::move(result.emoticons)));
}

}  // namespace ash
