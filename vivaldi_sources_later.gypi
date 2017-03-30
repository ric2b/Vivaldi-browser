# Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved
{
  # Always prepend filenames with "<(VIVALDI)/"
  # paths are relative to the *including* gypi file, not this file
  'variables': {
    # Source helper variables
    'vivaldi_grit_folder': '<(SHARED_INTERMEDIATE_DIR)/vivaldi',

    # Sources variables to be inserted into various targets,
    # either by direct reference in the target, or by using a
    # target condition in vivaldi_common to update the target
    #
    # Sources and target specific variables should be updated
    # using a target condtions.
    #
    # Dependencies and internal action related variables must be expanded
    # directly in the target, as target conditions cannot update
    # them; deps are evaluated before the conditions
    #
    # Full actions can be added by a target condition.

    #chrome_initial
    'vivaldi_add_mac_resources_chrome_exe' : [
      '<(VIVALDI)/app/resources/theme/<(VIVALDI_RELEASE_KIND)/mac/app.icns',
      '<(VIVALDI)/app/resources/theme/<(VIVALDI_RELEASE_KIND)/mac/document.icns',
    ],
  },
}
