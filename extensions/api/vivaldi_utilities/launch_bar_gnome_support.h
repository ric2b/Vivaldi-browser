// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include <optional>

namespace dock {

// Implements launch bar icon pinning for gnome.
class GnomeLaunchBar {
 public:
 GnomeLaunchBar();

 static bool IsGnomeRunning();
 static std::optional<bool> IsVivaldiPinned();
 static bool PinVivaldi();
};


} // namespace dock
