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

    'vivaldi_browser_tests_sources': [
       '<(VIVALDI)/extensions/api/webview/chrome_web_view_mediastate_apitest.cc',
    ],
    # chrome/chrome_resources.gyp:packed_extra_resources
    'packed_extra_resources_deps': [
      '<(VIVALDI)/extensions/vivaldi_api_resources.gyp:vivaldi_extension_resources',
      '<(VIVALDI)/app/vivaldi_resources.gyp:vivaldi_native_resources',
    ],
    # chrome/chrome_resources.gyp:packed_resources
    'packed_extra_strings_deps': [
      '<(VIVALDI)/app/vivaldi_resources.gyp:vivaldi_native_strings',
    ],
    'repack_extras_100_percent': [
      '<(vivaldi_grit_folder)/vivaldi_native_resources_100_percent.pak',
    ],
    'repack_extras_200_percent': [
      '<(vivaldi_grit_folder)/vivaldi_native_resources_200_percent.pak',
    ],
    # chrome/chrome_repack_resources.gypi
    'repack_extra_resources': [
      '<(vivaldi_grit_folder)/vivaldi_extension_resources.pak',
      '<(vivaldi_grit_folder)/vivaldi_native_unscaled.pak',
    ],
    'repack_extra_strings': [
       '-e', '<(SHARED_INTERMEDIATE_DIR)/vivaldi/vivaldi_native_strings',
    ],
    # chrome_main_dll, chrome_child_dll, chrome_initial, test_support_common
    'vivaldi_main_delegate' : [
      '<(VIVALDI)/extraparts/vivaldi_main_delegate.cpp',
      '<(VIVALDI)/extraparts/vivaldi_main_delegate.h',
    ],
    # test_support_common
    'vivaldi_unit_test_suite' : [
      '<(VIVALDI)/extraparts/vivaldi_unit_test_suite.cpp',
      '<(VIVALDI)/extraparts/vivaldi_unit_test_suite.h',
    ],
    # base
    'vivaldi_base': [
       '<(VIVALDI)/base/vivaldi_running.cpp',
       '<(VIVALDI)/app/vivaldi_apptools.h',
    ],
    # base_static, base_static_win64, base_i18n_nacl
    'vivaldi_base_static': [
       '<(VIVALDI)/app/vivaldi_apptools.h',
       '<(VIVALDI)/app/vivaldi_apptools.cpp',
       '<(VIVALDI)/app/vivaldi_constants.h',
       '<(VIVALDI)/app/vivaldi_constants.cc',
       '<(VIVALDI)/base/vivaldi_switches.cpp',
       '<(VIVALDI)/base/vivaldi_switches.h',
    ],
    # browser
    'vivaldi_browser_sources' : [
      '<(VIVALDI)/browser/mac/sparkle_util.h',
      '<(VIVALDI)/browser/mac/sparkle_util.mm',
      '<(VIVALDI)/browser/menus/vivaldi_menus.cpp',
      '<(VIVALDI)/browser/menus/vivaldi_menus.h',
      '<(VIVALDI)/browser/menus/vivaldi_menu_enums.h',
      #'<(SHARED_INTERMEDIATE_DIR)/vivaldi/grit/vivaldi_native_resources_map.cc
    ],
    # browser_ui
    'vivaldi_browser_ui': [
      '<(VIVALDI)/browser/startup_vivaldi_browser.cpp',
      '<(VIVALDI)/browser/startup_vivaldi_browser.h',
      '<(VIVALDI)/ui/vivaldi_browser_window.cc',
      '<(VIVALDI)/ui/vivaldi_browser_window.h',
      '<(VIVALDI)/ui/vivaldi_session_service.cc',
      '<(VIVALDI)/ui/vivaldi_session_service.h',
    ],
    'vivaldi_browser_ui_mac': [
      '<(VIVALDI)/ui/cocoa/vivaldi_browser_window_cocoa.h',
      '<(VIVALDI)/ui/cocoa/vivaldi_browser_window_cocoa.mm',
      '<(VIVALDI)/ui/cocoa/vivaldi_context_menu_mac.h',
      '<(VIVALDI)/ui/cocoa/vivaldi_context_menu_mac.mm',
      '<(VIVALDI)/ui/cocoa/vivaldi_main_menu_mac.h',
      '<(VIVALDI)/ui/cocoa/vivaldi_main_menu_mac.mm',
    ],
    'vivaldi_browser_ui_views': [
      '<(VIVALDI)/ui/views/vivaldi_context_menu_views.cc',
      '<(VIVALDI)/ui/views/vivaldi_context_menu_views.h',
      '<(VIVALDI)/ui/views/vivaldi_main_menu_views.cc',
    ],
    # setup
    'vivaldi_setup': [
      '<(VIVALDI)/installer/util/vivaldi_install_dialog.h',
      '<(VIVALDI)/installer/util/vivaldi_install_dialog.cc',
      '<(VIVALDI)/installer/util/vivaldi_progress_dialog.h',
      '<(VIVALDI)/installer/util/vivaldi_progress_dialog.cc',
    ],
    #gtest
    'vivaldi_gtest_updater': [
      '<(VIVALDI)/testing/disable_unittests_api.h',
      '<(VIVALDI)/testing/disable_unittests.cpp',
      '<(VIVALDI)/testing/disabled_unittests.h',
      '<(VIVALDI)/testing/disabled_unittests_win.h',
      '<(VIVALDI)/testing/disabled_unittests_mac.h',
      '<(VIVALDI)/testing/disabled_unittests_lin.h',
      '<(VIVALDI)/testing/disabled_unittests_win_mac.h',
      '<(VIVALDI)/testing/disabled_unittests_lin_mac.h',
      '<(VIVALDI)/testing/disabled_unittests_win_lin.h',
      '<(VIVALDI)/testing/permanent_disabled_unittests.h',
      '<(VIVALDI)/testing/permanent_disabled_unittests_win.h',
      '<(VIVALDI)/testing/permanent_disabled_unittests_mac.h',
      '<(VIVALDI)/testing/permanent_disabled_unittests_lin.h',
      '<(VIVALDI)/testing/permanent_disabled_unittests_win_mac.h',
      '<(VIVALDI)/testing/permanent_disabled_unittests_lin_mac.h',
      '<(VIVALDI)/testing/permanent_disabled_unittests_win_lin.h',
    ],
    #common_constants
    'vivaldi_common_constants' : [
      '<(VIVALDI)/browser/win/vivaldi_standalone.cpp',
      '<(VIVALDI)/browser/win/vivaldi_standalone.h',
    ],
    # chrome_dll, chrome_main_dll ()
    'vivaldi_nibs': [
      '<(VIVALDI)/app/nibs/VivaldiMainMenu.xib',
    ],
    # extensions_browser
    'vivaldi_extensions_browser': [
      '<(VIVALDI)/extensions/api/guest_view/vivaldi_web_view_guest.cpp',

    ],
    # extensions_renderer
    'vivaldi_extensions_renderer': [
      '<(VIVALDI)/extensions/vivaldi_script_dispatcher.cpp',
      '<(VIVALDI)/extensions/vivaldi_script_dispatcher.cpp',
    ],

    'vivaldi_keycode_translation': [
      '<(VIVALDI)/browser/keycode_translation.cpp',
      '<(VIVALDI)/browser/keycode_translation.h',
    ],
    # chrome_initial
    'vivaldi_remove_mac_resources_chrome_exe' : [
      'app/theme/vivaldi/mac/app.icns',
      'app/theme/vivaldi/mac/document.icns',
    ],
    #chrome_initial
    'vivaldi_add_mac_resources_chrome_exe' : [
      '<(VIVALDI)/app/resources/theme/vivaldi/mac/app.icns',
      '<(VIVALDI)/app/resources/theme/vivaldi/mac/document.icns',
    ],
  },
}
