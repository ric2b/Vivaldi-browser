// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#ifndef BROWSER_VIVALDI_PROFILE_IMPL_H_
#define BROWSER_VIVALDI_PROFILE_IMPL_H_

class Profile;

namespace vivaldi {
// true == profile is okay
bool VivaldiValidateProfile(Profile*);
void VivaldiInitProfile(Profile*);
}

#endif  // BROWSER_VIVALDI_PROFILE_IMPL_H_
