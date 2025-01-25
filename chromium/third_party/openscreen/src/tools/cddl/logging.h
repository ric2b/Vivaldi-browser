// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CDDL_LOGGING_H_
#define TOOLS_CDDL_LOGGING_H_

#include <stdio.h>
#include <stdlib.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

#define CHECK(condition) (condition) ? (void)0 : Logger::Abort(#condition)

#define CHECK_EQ(a, b) CHECK((a) == (b))
#define CHECK_NE(a, b) CHECK((a) != (b))
#define CHECK_LT(a, b) CHECK((a) < (b))
#define CHECK_LE(a, b) CHECK((a) <= (b))
#define CHECK_GT(a, b) CHECK((a) > (b))
#define CHECK_GE(a, b) CHECK((a) >= (b))

// TODO(crbug.com/openscreen/75):
// #1: This class has no state, so it doesn't need to be a singleton, just
// a collection of static functions.
//
// #2: Convert to stream oriented logging to clean up security warnings.
class Logger {
 public:
  // Writes a log to the global singleton instance of Logger.
  template <typename... Args>
  static void Log(const std::string& message) {
    Logger::Get()->WriteLog(message);
  }

  // Writes an error to the global singleton instance of Logger.
  template <typename... Args>
  static void Error(const std::string& message) {
    Logger::Get()->WriteError(message);
  }

  // Returns the singleton instance of Logger.
  static Logger* Get();

  // Aborts the program after logging the condition that caused the
  // CHECK-failure.
  static void Abort(const char* condition);

 private:
  // Creates and initializes the logging file associated with this logger.
  void InitializeInstance();

  // Limit calling the constructor/destructor to from within this same class.
  Logger();

  // Represents whether this instance has been initialized.
  bool is_initialized_;

  // Singleton instance of logger. At the beginning of runtime it's initiated to
  // nullptr due to zero initialization.
  static Logger* singleton_;

  // Exits the program if initialization has not occured.
  void VerifyInitialized();

  // fprintf doesn't like passing strings as parameters, so use overloads to
  // convert all C++ std::string types into C strings.
  template <class T>
  T MakePrintable(const T data) {
    return data;
  }

  const char* MakePrintable(const std::string& data);

  // Writes a log message to this instance of Logger's text file.
  void WriteToStream(const std::string& message) {
    VerifyInitialized();
    std::cerr << message << std::endl;
  }

  // Writes an error message.
  void WriteError(const std::string& message) {
    WriteToStream("Error: " + message);
  }

  // Writes a log message.
  void WriteLog(const std::string& message) { WriteToStream(message); }
};

#endif  // TOOLS_CDDL_LOGGING_H_
