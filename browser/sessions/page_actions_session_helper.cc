
#include "browser/sessions/page_actions_session_helper.h"

#include "app/vivaldi_apptools.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service.h"
#include "components/page_actions/page_actions_service_factory.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_command.h"
#include "components/sessions/vivaldi_session_service_commands.h"

void SessionService::VivPageActionOverridesChanged(
    const SessionID& window_id,
    const SessionID& tab_id,
    const base::FilePath& script_path,
    page_actions::Service::ScriptOverride script_override) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;
  switch (script_override) {
    case page_actions::Service::kNoOverride:
      ScheduleCommand(sessions::CreateRemoveVivPageActionOverrideCommand(
          tab_id, script_path.AsUTF8Unsafe()));
      break;
    case page_actions::Service::kEnabledOverride:
      ScheduleCommand(sessions::CreateVivPageActionOverrideCommand(
          tab_id, script_path.AsUTF8Unsafe(), true));
      break;
    case page_actions::Service::kDisabledOverride:
      ScheduleCommand(sessions::CreateVivPageActionOverrideCommand(
          tab_id, script_path.AsUTF8Unsafe(), false));
      break;
  }
}

PageActionsSessionHelper::PageActionsSessionHelper(
    SessionService* session_service)
    : session_service_(session_service), profile_(session_service_->profile()) {
  if (!profile_ ||
      (!vivaldi::IsVivaldiRunning() && !vivaldi::ForcedVivaldiRunning())) {
    return;
  }
  profile_->AddObserver(this);
  page_actions::ServiceFactory::GetForBrowserContext(profile_)->AddObserver(
      this);
}

PageActionsSessionHelper::~PageActionsSessionHelper() {
  OnProfileWillBeDestroyed(profile_);
}

void PageActionsSessionHelper::OnScriptOverridesChanged(
    content::WebContents* tab_contents,
    const base::FilePath& script_path,
    page_actions::Service::ScriptOverride script_override) {
  sessions::SessionTabHelper* session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(tab_contents);
  if (!session_tab_helper)
    return;
  session_service_->VivPageActionOverridesChanged(
      session_tab_helper->window_id(), session_tab_helper->session_id(),
      script_path, script_override);
}

void PageActionsSessionHelper::OnProfileWillBeDestroyed(Profile* profile) {
  if (!profile_)
    return;

  DCHECK(profile == profile_);
  profile_->RemoveObserver(this);

  // The page_actions is not started for disable-vivaldi.
  if ((vivaldi::IsVivaldiRunning() || vivaldi::ForcedVivaldiRunning())) {
    auto* page_action_service =
        page_actions::ServiceFactory::GetForBrowserContextIfExists(profile_);
    if (page_action_service)
      page_action_service->RemoveObserver(this);
  }
  profile_ = nullptr;
}
