// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include <string>

// NOTE(yngve) using relative directory for include because
// chromium is not in the standard gtest include path
#include "../chromium/base/macros.h"
#include "../chromium/build/build_config.h"

#include "testing/disable_unittests_api.h"

#include "testing/disable_unittests_macros.h"

namespace {

struct disabled_test_config {
  const char* testcase;
  const char* testname;
  bool looped;
};
const disabled_test_config disabled_tests_list[] = {
    {NULL, NULL, false},  // Placeholder, the array need at least one element
#if !defined(DONT_DISABLE_UNITTESTS)
#include "testing/disabled_unittests.h"
#if defined(OS_WIN)
#include "testing/disabled_unittests_win.h"
#endif
#if defined(OS_MACOSX)
#include "testing/disabled_unittests_mac.h"
#endif
#if defined(OS_LINUX)
#include "testing/disabled_unittests_lin.h"
#endif
#if defined(OS_MACOSX) || defined(OS_WIN)
#include "testing/disabled_unittests_win_mac.h"
#endif
#if defined(OS_MACOSX) || defined(OS_LINUX)
#include "testing/disabled_unittests_lin_mac.h"
#endif
#if defined(OS_LINUX) || defined(OS_WIN)
#include "testing/disabled_unittests_win_lin.h"
#endif
#endif  // DONT_DISABLE_UNITTESTS
#if !defined(DONT_PERMANENTLY_DISABLE_UNITTESTS)
#include "testing/permanent_disabled_unittests.h"
#if defined(OS_WIN)
#include "testing/permanent_disabled_unittests_win.h"
#endif
#if defined(OS_MACOSX)
#include "testing/permanent_disabled_unittests_mac.h"
#endif
#if defined(OS_LINUX)
#include "testing/permanent_disabled_unittests_lin.h"
#endif
#if defined(OS_MACOSX) || defined(OS_WIN)
#include "testing/permanent_disabled_unittests_win_mac.h"
#endif
#if defined(OS_MACOSX) || defined(OS_LINUX)
#include "testing/permanent_disabled_unittests_lin_mac.h"
#endif
#if defined(OS_LINUX) || defined(OS_WIN)
#include "testing/permanent_disabled_unittests_win_lin.h"
#endif
#endif  // DONT_PERMANENTLY_DISABLE_UNITTESTS
};
}  // namespace

// NOTE(yngve): This code does not use a preprocessed map<string, set<string>>
// to store the information since the map library is not fully initialized
// before this function is called. A possibility for optimizing is to require
// the list to be sorted.
void UpdateNamesOfDisabledTests(std::string* a_test_case_name,
                                std::string* a_name) {
  // Starts at index 1, index 0 is a placeholder to avoid compile issues
  for (size_t i = 1; i < arraysize(disabled_tests_list); i++) {
    if (disabled_tests_list[i].looped) {
      // A looped entry is on the form [foo/]bar.baz/1 ; with bar.baz being the
      // disabled entry
      size_t pos = a_test_case_name->find('/');
      if (pos != std::string::npos) {
        if (!disabled_tests_list[i].testname &&
            a_test_case_name->compare(0, pos,
                                      disabled_tests_list[i].testcase) == 0) {
          a_test_case_name->insert(0, "DISABLED_");
          return;
        }
        pos++;
      } else {
        pos = 0;
      }

      if (a_test_case_name->compare(pos, std::string::npos,
                                    disabled_tests_list[i].testcase) != 0)
        continue;

      if (!disabled_tests_list[i].testname) {
        a_test_case_name->insert(0, "DISABLED_");
        return;
      }
      pos = a_name->find('/');

      if (a_name->compare(0, pos, disabled_tests_list[i].testname) == 0) {
        a_name->insert(0, "DISABLED_");
        return;
      }
    } else {
      if (a_test_case_name->compare(disabled_tests_list[i].testcase) != 0)
        continue;

      if (!disabled_tests_list[i].testname) {
        a_test_case_name->insert(0, "DISABLED_");
        return;
      } else if (a_name->compare(disabled_tests_list[i].testname) == 0) {
        a_name->insert(0, "DISABLED_");
        return;
      }
    }
  }
}
