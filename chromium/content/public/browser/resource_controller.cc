// Copyright (c) 2016 Vivaldi Technologies

#include "content/public/browser/resource_controller.h"

namespace content {

/* NOTE(yngve): Risk of infinite loop, should only be a problem for us,
 * if we add new subclasses, chromium will still use the abstract definition.
 */
void ResourceController::Resume() {
  Resume(false, false);
}

void ResourceController::Resume(bool open_when_done,
                      bool ask_for_target) {
  Resume();
};

}  // namespace content
