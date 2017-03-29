{
  # Always prepend filenames with "<(VIVALDI)/"
  # paths are relative to the *including* gypi file, not this file
  'variables': {
    # Source helper variables

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
  },
}
