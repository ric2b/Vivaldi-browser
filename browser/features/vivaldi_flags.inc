// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

// This file is included to chromium/chrome/browser/about_flags.cc and
// chromium/ios/chrome/browser/flags/about_flags.mm at the end of the definition
// of kFeatureEntries.

// Instructions on how to add new feature flags:
// https://wiki.viv.int/doku.php?id=featuresystem#the_feature_flag_system

    {vivaldi_flag::kChromePagesInternalName, vivaldi_flag::kChromePagesName,
    vivaldi_flag::kChromePagesDescription,
    flags_ui::kOsMac | flags_ui::kOsWin | flags_ui::kOsLinux | flags_ui::kOsCrOS,
    FEATURE_VALUE_TYPE(vivaldi_features::kChromePages)},

    {vivaldi_flag::kCssModsInternalName, vivaldi_flag::kCssModsName,
     vivaldi_flag::kCssModsDescription,
     flags_ui::kOsMac | flags_ui::kOsWin | flags_ui::kOsLinux |
         flags_ui::kOsCrOS,
     FEATURE_VALUE_TYPE(vivaldi_features::kCssMods)},

    {vivaldi_flag::kDoubleClickMenuInternalName,
     vivaldi_flag::kDoubleClickMenuName,
     vivaldi_flag::kDoubleClickMenuDescription,
     flags_ui::kOsMac | flags_ui::kOsWin | flags_ui::kOsLinux |
         flags_ui::kOsCrOS,
     FEATURE_VALUE_TYPE(vivaldi_features::kDoubleClickMenu)},

    {vivaldi_flag::kInternalPageReaderModeInternalName,
     vivaldi_flag::kInternalPageReaderModeName,
     vivaldi_flag::kInternalPageReaderModeDescription,
     flags_ui::kOsMac | flags_ui::kOsWin | flags_ui::kOsLinux |
         flags_ui::kOsCrOS,
     FEATURE_VALUE_TYPE(vivaldi_features::kInternalPageReaderMode)},

    {vivaldi_flag::kLocationOverrideInternalName,
     vivaldi_flag::kLocationOverrideName,
     vivaldi_flag::kLocationOverrideDescription,
     flags_ui::kOsMac | flags_ui::kOsWin | flags_ui::kOsLinux |
         flags_ui::kOsCrOS,
     FEATURE_VALUE_TYPE(vivaldi_features::kLocationOverride)},

    {vivaldi_flag::kNewPrivacyReportInternalName,
     vivaldi_flag::kNewPrivacyReportName,
     vivaldi_flag::kNewPrivacyReportDescription,
     flags_ui::kOsMac | flags_ui::kOsWin | flags_ui::kOsLinux |
         flags_ui::kOsCrOS | flags_ui::kOsAndroid,
     FEATURE_VALUE_TYPE(vivaldi_features::kNewPrivacyReport)},

    {vivaldi_flag::kShowNewSpeedDialDialogInternalName,
     vivaldi_flag::kShowNewSpeedDialDialogName,
     vivaldi_flag::kShowNewSpeedDialDialogDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(vivaldi_features::kShowNewSpeedDialDialog)},

    {vivaldi_flag::kShowTopSitesInternalName, vivaldi_flag::kShowTopSitesName,
     vivaldi_flag::kShowTopSitesDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(vivaldi_features::kShowTopSites)},

    {vivaldi_flag::kSpeeddialWidgetsInternalName,
     vivaldi_flag::kSpeeddialWidgetsName,
     vivaldi_flag::kSpeeddialWidgetsDescription,
     flags_ui::kOsMac | flags_ui::kOsWin | flags_ui::kOsLinux |
         flags_ui::kOsCrOS,
     FEATURE_VALUE_TYPE(vivaldi_features::kSpeeddialWidgets)},

    {vivaldi_flag::kTabsButtonInternalName, vivaldi_flag::kTabsButtonName,
     vivaldi_flag::kTabsButtonDescription,
     flags_ui::kOsMac | flags_ui::kOsWin | flags_ui::kOsLinux |
         flags_ui::kOsCrOS,
     FEATURE_VALUE_TYPE(vivaldi_features::kTabsButton)},

    {vivaldi_flag::kViewMarkdownAsHTMLInternalName,
     vivaldi_flag::kViewMarkdownAsHTMLName,
     vivaldi_flag::kkViewMarkdownAsHTMLDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(vivaldi_features::kViewMarkdownAsHTML)},

    {vivaldi_flag::kAddCustomSearchEngineOptionInternalName,
     vivaldi_flag::kAddCustomSearchEngineOption,
     vivaldi_flag::kAddCustomSearchEngineOptionDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(vivaldi_features::kAddCustomSearchEngineOption)},

    {vivaldi_flag::kNoteEditorName, vivaldi_flag::kNoteEditorOption,
     vivaldi_flag::kNoteEditorDescription,
     flags_ui::kOsMac | flags_ui::kOsWin | flags_ui::kOsLinux |
         flags_ui::kOsCrOS,
     FEATURE_VALUE_TYPE(vivaldi_features::kNoteEditor)},

#if defined(OEM_AUTOMOTIVE_BUILD)
    {vivaldi_flag::kCinemaModeInternalName, vivaldi_flag::kCinemaModeName,
     vivaldi_flag::kCinemaModeDescription, flags_ui::kOsAndroid,
     FEATURE_VALUE_TYPE(vivaldi_features::kCinemaMode)},
#endif  // defined(OEM_AUTOMOTIVE_BUILD)
