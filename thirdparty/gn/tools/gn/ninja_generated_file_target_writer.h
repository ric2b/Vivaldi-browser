// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_GENERATED_FILE_TARGET_WRITER_H_
#define TOOLS_GN_NINJA_GENERATED_FILE_TARGET_WRITER_H_

#include "base/macros.h"
#include "tools/gn/ninja_target_writer.h"

// Writes a .ninja file for a group target type.
class NinjaGeneratedFileTargetWriter : public NinjaTargetWriter {
 public:
  NinjaGeneratedFileTargetWriter(const Target* target, std::ostream& out);
  ~NinjaGeneratedFileTargetWriter() override;

  void Run() override;

 private:
  void GenerateFile();

  DISALLOW_COPY_AND_ASSIGN(NinjaGeneratedFileTargetWriter);
};

#endif  // TOOLS_GN_NINJA_GENERATED_FILE_TARGET_WRITER_H_
