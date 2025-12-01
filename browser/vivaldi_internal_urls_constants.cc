// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

namespace vivaldi {

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
base::span<const base::cstring_view> VivaldiURLHosts() {
  static constexpr auto kVivaldiURLHosts =
      std::to_array<base::cstring_view>({"version", "about", "game"});
  return base::span(kVivaldiURLHosts);
}
#else
base::span<const base::cstring_view> VivaldiURLHosts() {
  static constexpr auto kVivaldiURLHosts = std::to_array<base::cstring_view>(
      {"version", "help", "about", "welcome", "notes", "bookmarks", "history",
       "settings", "extensions", "experiments", "game"});
  return base::span(kVivaldiURLHosts);
}
#endif
}  // namespace vivaldi
