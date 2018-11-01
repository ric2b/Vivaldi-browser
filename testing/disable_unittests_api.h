// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef TESTING_DISABLE_UNITTESTS_API_H_
#define TESTING_DISABLE_UNITTESTS_API_H_

#include <string>

// NOTE(yngve): This code does not use a preprocessed map<string, set<string>>
// to store the information since the map library is not fully initialized
// before this function is called. A possibility for optimizing is to require
// the list to be sorted.
void UpdateNamesOfDisabledTests(std::string* a_test_case_name,
                                std::string* a_name);

#endif  // TESTING_DISABLE_UNITTESTS_API_H_
