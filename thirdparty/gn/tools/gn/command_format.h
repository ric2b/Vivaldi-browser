// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_COMAND_FORMAT_H_
#define TOOLS_GN_COMAND_FORMAT_H_

#include <string>

class Setup;
class SourceFile;

namespace commands {

enum class TreeDumpMode {
  // Normal operation mode. Format the input file.
  kInactive,

  // Output the token tree with indented plain text. For debugging.
  kPlainText,

  // Output the token tree in JSON format. Used for exporting a tree to another
  // program.
  kJSON
};

bool FormatFileToString(Setup* setup,
                        const SourceFile& file,
                        TreeDumpMode dump_tree,
                        std::string* output);

bool FormatStringToString(const std::string& input,
                          TreeDumpMode dump_tree,
                          std::string* output);

}  // namespace commands

#endif  // TOOLS_GN_COMAND_FORMAT_H_
