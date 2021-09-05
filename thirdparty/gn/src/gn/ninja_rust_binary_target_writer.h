// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_RUST_BINARY_TARGET_WRITER_H_
#define TOOLS_GN_NINJA_RUST_BINARY_TARGET_WRITER_H_

#include "base/macros.h"
#include "gn/ninja_binary_target_writer.h"
#include "gn/rust_tool.h"

struct EscapeOptions;

// Writes a .ninja file for a binary target type (an executable, a shared
// library, or a static library).
class NinjaRustBinaryTargetWriter : public NinjaBinaryTargetWriter {
 public:
  NinjaRustBinaryTargetWriter(const Target* target, std::ostream& out);
  ~NinjaRustBinaryTargetWriter() override;

  void Run() override;

 private:
  void WriteCompilerVars();
  void WriteSources(const OutputFile& input_dep,
                    const std::vector<OutputFile>& order_only_deps);
  void WriteExterns(const std::vector<const Target*>& deps);
  void WriteRustdeps(const std::vector<OutputFile>& transitive_rustdeps,
      const std::vector<OutputFile>& rustdeps,
      const std::vector<OutputFile>& nonrustdeps);
  void WriteEdition();

  const RustTool* tool_;

  DISALLOW_COPY_AND_ASSIGN(NinjaRustBinaryTargetWriter);
};

#endif  // TOOLS_GN_NINJA_RUST_BINARY_TARGET_WRITER_H_
