// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_TEST_UTIL_H_
#define ASH_WM_OVERVIEW_OVERVIEW_TEST_UTIL_H_

#include "ash/wm/overview/overview_session.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace views {
class View;
}  // namespace views

namespace ui::test {
class EventGenerator;
}  // namespace ui::test

namespace ash {

class OverviewGrid;
class OverviewItemBase;

// Focuses `window` in the active overview session by cycling through all
// windows in overview until it is found. Returns true if `window` was found,
// false otherwise.
bool FocusOverviewWindow(const aura::Window* window,
                         ui::test::EventGenerator* event_generator);

// Gets the current focused window. Returns nullptr if no window is focused.
const aura::Window* GetOverviewFocusedWindow();

void ToggleOverview(
    OverviewEnterExitType type = OverviewEnterExitType::kNormal);

// Waits for the overview enter/exit animations to finish. No-op and immediately
// returns if animations are disabled.
void WaitForOverviewEnterAnimation();
void WaitForOverviewExitAnimation();

// Like `WaitForOverviewEnterAnimation()` but waits even if using zero duration.
// Used to wait for async pine image read operation.
void WaitForOverviewEntered();

OverviewGrid* GetOverviewGridForRoot(aura::Window* root);

const std::vector<std::unique_ptr<OverviewItemBase>>& GetOverviewItemsForRoot(
    int index);

std::vector<aura::Window*> GetWindowsListInOverviewGrids();

// Returns the OverviewItem associated with |window| if it exists.
OverviewItemBase* GetOverviewItemForWindow(aura::Window* window);

// If `drop` is false, the dragged `item` won't be dropped; giving the caller
// a chance to do some validations before the item is dropped.
void DragItemToPoint(OverviewItemBase* item,
                     const gfx::Point& screen_location,
                     ui::test::EventGenerator* event_generator,
                     bool by_touch_gestures = false,
                     bool drop = true);

// Press the key repeatedly until a window is focused, i.e. ignoring any
// desk items.
void SendKeyUntilOverviewItemIsFocused(
    ui::KeyboardCode key,
    ui::test::EventGenerator* event_generator);

// Waits until the occlusion state for window is equal to `target_state`.
void WaitForOcclusionStateChange(aura::Window* window,
                                 aura::Window::OcclusionState target_state);

// Returns true if the given `window` is on its corresponding overview grid,
// returns false otherwise.
bool IsWindowInItsCorrespondingOverviewGrid(aura::Window* window);

views::View* GetFocusedView();

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_TEST_UTIL_H_
