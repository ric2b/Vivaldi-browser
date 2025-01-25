// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "extraparts/vivaldi_browser_main_extra_parts_android.h"

#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile_manager.h"

#include "browser/vivaldi_default_bookmarks.h"
#include "browser/vivaldi_default_bookmarks_updater_client_impl.h"
#include "components/direct_match/direct_match_service_factory.h"

VivaldiBrowserMainExtraPartsAndroid::VivaldiBrowserMainExtraPartsAndroid() =
    default;

VivaldiBrowserMainExtraPartsAndroid::~VivaldiBrowserMainExtraPartsAndroid() =
    default;

void VivaldiBrowserMainExtraPartsAndroid::PostProfileInit(
    Profile* profile,
    bool is_initial_profile) {
  VivaldiBrowserMainExtraParts::PostProfileInit(profile, is_initial_profile);

  if (first_run::IsChromeFirstRun()) {
    DCHECK(profile);
    vivaldi_default_bookmarks::UpdatePartners(
        vivaldi_default_bookmarks::UpdaterClientImpl::Create(profile));
  }
    direct_match::DirectMatchServiceFactory::GetInstance();
}

//void VivaldiBrowserMainExtraPartsAndroid::PreProfileInit() {
  //EnsureBrowserContextKeyedServiceFactoriesBuilt();
//}

//void VivaldiBrowserMainExtraPartsAndroid::
    //EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  //direct_match::DirectMatchServiceFactory::GetInstance();
//}

std::unique_ptr<VivaldiBrowserMainExtraParts>
VivaldiBrowserMainExtraParts::Create() {
  return std::make_unique<VivaldiBrowserMainExtraPartsAndroid>();
}
