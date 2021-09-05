// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/ninja_rust_binary_target_writer.h"

#include "gn/config.h"
#include "gn/rust_values.h"
#include "gn/scheduler.h"
#include "gn/target.h"
#include "gn/test_with_scheduler.h"
#include "gn/test_with_scope.h"
#include "util/build_config.h"
#include "util/test/test.h"

using NinjaRustBinaryTargetWriterTest = TestWithScheduler;

TEST_F(NinjaRustBinaryTargetWriterTest, RustSourceSet) {
  Err err;
  TestWithScope setup;

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::SOURCE_SET);
  target.visibility().SetPublic();
  target.sources().push_back(SourceFile("//foo/input1.rs"));
  target.sources().push_back(SourceFile("//foo/main.rs"));
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  // Source set itself.
  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build obj/foo/bar.stamp: stamp ../../foo/input1.rs "
        "../../foo/main.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, RustExecutable) {
  Err err;
  TestWithScope setup;

  Target source_set(setup.settings(), Label(SourceDir("//foo/"), "sources"));
  source_set.set_output_type(Target::SOURCE_SET);
  source_set.visibility().SetPublic();
  source_set.sources().push_back(SourceFile("//foo/input1.rs"));
  source_set.sources().push_back(SourceFile("//foo/input2.rs"));
  source_set.source_types_used().Set(SourceFile::SOURCE_RS);
  source_set.SetToolchain(setup.toolchain());
  ASSERT_TRUE(source_set.OnResolved(&err));

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/input3.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  target.private_deps().push_back(LabelTargetPair(&source_set));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_extension = \n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build ./foo_bar: rust_bin ../../foo/main.rs | ../../foo/input3.rs "
        "../../foo/main.rs ../../foo/input1.rs ../../foo/input2.rs || "
        "obj/foo/sources.stamp\n"
        "  externs =\n"
        "  rustdeps =\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, RlibDeps) {
  Err err;
  TestWithScope setup;

  Target rlib(setup.settings(), Label(SourceDir("//bar/"), "mylib"));
  rlib.set_output_type(Target::RUST_LIBRARY);
  rlib.visibility().SetPublic();
  SourceFile barlib("//bar/lib.rs");
  rlib.sources().push_back(SourceFile("//bar/mylib.rs"));
  rlib.sources().push_back(barlib);
  rlib.source_types_used().Set(SourceFile::SOURCE_RS);
  rlib.rust_values().set_crate_root(barlib);
  rlib.rust_values().crate_name() = "mylib";
  rlib.SetToolchain(setup.toolchain());
  ASSERT_TRUE(rlib.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&rlib, out);
    writer.Run();

    const char expected[] =
        "crate_name = mylib\n"
        "crate_type = rlib\n"
        "output_extension = .rlib\n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/bar\n"
        "target_output_name = libmylib\n"
        "\n"
        "build obj/bar/libmylib.rlib: rust_rlib ../../bar/lib.rs | "
        "../../bar/mylib.rs ../../bar/lib.rs\n"
        "  externs =\n"
        "  rustdeps =\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }

  Target another_rlib(setup.settings(), Label(SourceDir("//foo/"), "direct"));
  another_rlib.set_output_type(Target::RUST_LIBRARY);
  another_rlib.visibility().SetPublic();
  SourceFile lib("//foo/main.rs");
  another_rlib.sources().push_back(SourceFile("//foo/direct.rs"));
  another_rlib.sources().push_back(lib);
  another_rlib.source_types_used().Set(SourceFile::SOURCE_RS);
  another_rlib.rust_values().set_crate_root(lib);
  another_rlib.rust_values().crate_name() = "direct";
  another_rlib.SetToolchain(setup.toolchain());
  another_rlib.public_deps().push_back(LabelTargetPair(&rlib));
  ASSERT_TRUE(another_rlib.OnResolved(&err));

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/source.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  target.private_deps().push_back(LabelTargetPair(&another_rlib));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_extension = \n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build ./foo_bar: rust_bin ../../foo/main.rs | ../../foo/source.rs "
        "../../foo/main.rs obj/foo/libdirect.rlib\n"
        "  externs = --extern direct=obj/foo/libdirect.rlib\n"
        "  rustdeps = -Ldependency=obj/foo -Ldependency=obj/bar\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, RlibDepsAcrossGroups) {
  Err err;
  TestWithScope setup;

  Target procmacro(setup.settings(), Label(SourceDir("//bar/"), "mymacro"));
  procmacro.set_output_type(Target::RUST_PROC_MACRO);
  procmacro.visibility().SetPublic();
  SourceFile barproc("//bar/lib.rs");
  procmacro.sources().push_back(SourceFile("//bar/mylib.rs"));
  procmacro.sources().push_back(barproc);
  procmacro.source_types_used().Set(SourceFile::SOURCE_RS);
  procmacro.rust_values().set_crate_root(barproc);
  procmacro.rust_values().crate_name() = "mymacro";
  procmacro.rust_values().set_crate_type(RustValues::CRATE_PROC_MACRO);
  procmacro.SetToolchain(setup.toolchain());
  ASSERT_TRUE(procmacro.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&procmacro, out);
    writer.Run();

    const char expected[] =
        "crate_name = mymacro\n"
        "crate_type = proc-macro\n"
        "output_extension = .so\n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/bar\n"
        "target_output_name = libmymacro\n"
        "\n"
        "build obj/bar/libmymacro.so: rust_macro ../../bar/lib.rs | "
        "../../bar/mylib.rs ../../bar/lib.rs\n"
        "  externs =\n"
        "  rustdeps =\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }

  Target group(setup.settings(), Label(SourceDir("//baz/"), "group"));
  group.set_output_type(Target::GROUP);
  group.visibility().SetPublic();
  group.public_deps().push_back(LabelTargetPair(&procmacro));
  group.SetToolchain(setup.toolchain());
  ASSERT_TRUE(group.OnResolved(&err));

  Target rlib(setup.settings(), Label(SourceDir("//bar/"), "mylib"));
  rlib.set_output_type(Target::RUST_LIBRARY);
  rlib.visibility().SetPublic();
  SourceFile barlib("//bar/lib.rs");
  rlib.sources().push_back(SourceFile("//bar/mylib.rs"));
  rlib.sources().push_back(barlib);
  rlib.source_types_used().Set(SourceFile::SOURCE_RS);
  rlib.rust_values().set_crate_root(barlib);
  rlib.rust_values().crate_name() = "mylib";
  rlib.SetToolchain(setup.toolchain());
  rlib.public_deps().push_back(LabelTargetPair(&group));
  ASSERT_TRUE(rlib.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&rlib, out);
    writer.Run();

    const char expected[] =
        "crate_name = mylib\n"
        "crate_type = rlib\n"
        "output_extension = .rlib\n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/bar\n"
        "target_output_name = libmylib\n"
        "\n"
        "build obj/bar/libmylib.rlib: rust_rlib ../../bar/lib.rs | "
        "../../bar/mylib.rs ../../bar/lib.rs obj/bar/libmymacro.so || "
        "obj/baz/group.stamp\n"
        "  externs = --extern mymacro=obj/bar/libmymacro.so\n"
        "  rustdeps = -Ldependency=obj/bar\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/source.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  target.private_deps().push_back(LabelTargetPair(&rlib));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_extension = \n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build ./foo_bar: rust_bin ../../foo/main.rs | "
        "../../foo/source.rs ../../foo/main.rs obj/bar/libmylib.rlib\n"
        "  externs = --extern mylib=obj/bar/libmylib.rlib\n"
        "  rustdeps = -Ldependency=obj/bar -Ldependency=obj/bar\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, RenamedDeps) {
  Err err;
  TestWithScope setup;

  Target another_rlib(setup.settings(), Label(SourceDir("//foo/"), "direct"));
  another_rlib.set_output_type(Target::RUST_LIBRARY);
  another_rlib.visibility().SetPublic();
  SourceFile lib("//foo/lib.rs");
  another_rlib.sources().push_back(SourceFile("//foo/direct.rs"));
  another_rlib.sources().push_back(lib);
  another_rlib.source_types_used().Set(SourceFile::SOURCE_RS);
  another_rlib.rust_values().set_crate_root(lib);
  another_rlib.rust_values().crate_name() = "direct";
  another_rlib.SetToolchain(setup.toolchain());
  ASSERT_TRUE(another_rlib.OnResolved(&err));

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/source.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  target.rust_values().aliased_deps()[another_rlib.label()] = "direct_renamed";
  target.private_deps().push_back(LabelTargetPair(&another_rlib));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_extension = \n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build ./foo_bar: rust_bin ../../foo/main.rs | ../../foo/source.rs "
        "../../foo/main.rs obj/foo/libdirect.rlib\n"
        "  externs = --extern direct_renamed=obj/foo/libdirect.rlib\n"
        "  rustdeps = -Ldependency=obj/foo\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, NonRustDeps) {
  Err err;
  TestWithScope setup;

  Target rlib(setup.settings(), Label(SourceDir("//bar/"), "mylib"));
  rlib.set_output_type(Target::RUST_LIBRARY);
  rlib.visibility().SetPublic();
  SourceFile barlib("//bar/lib.rs");
  rlib.sources().push_back(SourceFile("//bar/mylib.rs"));
  rlib.sources().push_back(barlib);
  rlib.source_types_used().Set(SourceFile::SOURCE_RS);
  rlib.rust_values().set_crate_root(barlib);
  rlib.rust_values().crate_name() = "mylib";
  rlib.SetToolchain(setup.toolchain());
  ASSERT_TRUE(rlib.OnResolved(&err));

  Target staticlib(setup.settings(), Label(SourceDir("//foo/"), "static"));
  staticlib.set_output_type(Target::STATIC_LIBRARY);
  staticlib.visibility().SetPublic();
  staticlib.sources().push_back(SourceFile("//foo/static.cpp"));
  staticlib.source_types_used().Set(SourceFile::SOURCE_CPP);
  staticlib.SetToolchain(setup.toolchain());
  ASSERT_TRUE(staticlib.OnResolved(&err));

  Target sharedlib(setup.settings(), Label(SourceDir("//foo/"), "shared"));
  sharedlib.set_output_type(Target::SHARED_LIBRARY);
  sharedlib.visibility().SetPublic();
  sharedlib.sources().push_back(SourceFile("//foo/static.cpp"));
  sharedlib.source_types_used().Set(SourceFile::SOURCE_CPP);
  sharedlib.SetToolchain(setup.toolchain());
  ASSERT_TRUE(sharedlib.OnResolved(&err));

  Toolchain toolchain_with_toc(
      setup.settings(), Label(SourceDir("//toolchain_with_toc/"), "with_toc"));
  TestWithScope::SetupToolchain(&toolchain_with_toc, true);
  Target sharedlib_with_toc(setup.settings(),
                            Label(SourceDir("//foo/"), "shared_with_toc"));
  sharedlib_with_toc.set_output_type(Target::SHARED_LIBRARY);
  sharedlib_with_toc.visibility().SetPublic();
  sharedlib_with_toc.sources().push_back(SourceFile("//foo/static.cpp"));
  sharedlib_with_toc.source_types_used().Set(SourceFile::SOURCE_CPP);
  sharedlib_with_toc.SetToolchain(&toolchain_with_toc);
  ASSERT_TRUE(sharedlib_with_toc.OnResolved(&err));

  Target nonrust(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  nonrust.set_output_type(Target::EXECUTABLE);
  nonrust.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  nonrust.sources().push_back(SourceFile("//foo/source.rs"));
  nonrust.sources().push_back(main);
  nonrust.source_types_used().Set(SourceFile::SOURCE_RS);
  nonrust.rust_values().set_crate_root(main);
  nonrust.rust_values().crate_name() = "foo_bar";
  nonrust.private_deps().push_back(LabelTargetPair(&rlib));
  nonrust.private_deps().push_back(LabelTargetPair(&staticlib));
  nonrust.private_deps().push_back(LabelTargetPair(&sharedlib));
  nonrust.private_deps().push_back(LabelTargetPair(&sharedlib_with_toc));
  nonrust.SetToolchain(setup.toolchain());
  ASSERT_TRUE(nonrust.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&nonrust, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_extension = \n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build ./foo_bar: rust_bin ../../foo/main.rs | ../../foo/source.rs "
        "../../foo/main.rs obj/bar/libmylib.rlib obj/foo/libstatic.a "
        "./libshared.so ./libshared_with_toc.so.TOC\n"
        "  externs = --extern mylib=obj/bar/libmylib.rlib\n"
        "  rustdeps = -Ldependency=obj/bar -Lnative=obj/foo -lstatic "
        "-Lnative=. -lshared -Lnative=. -lshared_with_toc\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }

  Target nonrust_only(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  nonrust_only.set_output_type(Target::EXECUTABLE);
  nonrust_only.visibility().SetPublic();
  nonrust_only.sources().push_back(SourceFile("//foo/source.rs"));
  nonrust_only.sources().push_back(main);
  nonrust_only.source_types_used().Set(SourceFile::SOURCE_RS);
  nonrust_only.rust_values().set_crate_root(main);
  nonrust_only.rust_values().crate_name() = "foo_bar";
  nonrust_only.private_deps().push_back(LabelTargetPair(&staticlib));
  nonrust_only.SetToolchain(setup.toolchain());
  ASSERT_TRUE(nonrust_only.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&nonrust_only, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_extension = \n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build ./foo_bar: rust_bin ../../foo/main.rs | ../../foo/source.rs "
        "../../foo/main.rs obj/foo/libstatic.a\n"
        "  externs =\n"
        "  rustdeps = -Lnative=obj/foo -lstatic\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, RustOutputExtensionAndDir) {
  Err err;
  TestWithScope setup;

  Target source_set(setup.settings(), Label(SourceDir("//foo/"), "sources"));
  source_set.set_output_type(Target::SOURCE_SET);
  source_set.visibility().SetPublic();
  source_set.sources().push_back(SourceFile("//foo/input1.rs"));
  source_set.sources().push_back(SourceFile("//foo/input2.rs"));
  source_set.source_types_used().Set(SourceFile::SOURCE_RS);
  source_set.SetToolchain(setup.toolchain());
  ASSERT_TRUE(source_set.OnResolved(&err));

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/input3.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.set_output_extension(std::string("exe"));
  target.set_output_dir(SourceDir("//out/Debug/foo/"));
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  target.private_deps().push_back(LabelTargetPair(&source_set));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_extension = .exe\n"
        "output_dir = foo\n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build ./foo_bar.exe: rust_bin ../../foo/main.rs | ../../foo/input3.rs "
        "../../foo/main.rs ../../foo/input1.rs ../../foo/input2.rs || "
        "obj/foo/sources.stamp\n"
        "  externs =\n"
        "  rustdeps =\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, LibsAndLibDirs) {
  Err err;
  TestWithScope setup;

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/input.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.set_output_dir(SourceDir("//out/Debug/foo/"));
  target.config_values().libs().push_back(LibFile("quux"));
  target.config_values().lib_dirs().push_back(SourceDir("//baz/"));
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_extension = \n"
        "output_dir = foo\n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build ./foo_bar: rust_bin ../../foo/main.rs | ../../foo/input.rs "
        "../../foo/main.rs\n"
        "  externs =\n"
        "  rustdeps = -Lnative=../../baz -lquux\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, RustProcMacro) {
  Err err;
  TestWithScope setup;

  Target procmacro(setup.settings(), Label(SourceDir("//bar/"), "mymacro"));
  procmacro.set_output_type(Target::RUST_PROC_MACRO);
  procmacro.visibility().SetPublic();
  SourceFile barlib("//bar/lib.rs");
  procmacro.sources().push_back(SourceFile("//bar/mylib.rs"));
  procmacro.sources().push_back(barlib);
  procmacro.source_types_used().Set(SourceFile::SOURCE_RS);
  procmacro.rust_values().set_crate_root(barlib);
  procmacro.rust_values().crate_name() = "mymacro";
  procmacro.rust_values().set_crate_type(RustValues::CRATE_PROC_MACRO);
  procmacro.SetToolchain(setup.toolchain());
  ASSERT_TRUE(procmacro.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&procmacro, out);
    writer.Run();

    const char expected[] =
        "crate_name = mymacro\n"
        "crate_type = proc-macro\n"
        "output_extension = .so\n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/bar\n"
        "target_output_name = libmymacro\n"
        "\n"
        "build obj/bar/libmymacro.so: rust_macro ../../bar/lib.rs | "
        "../../bar/mylib.rs ../../bar/lib.rs\n"
        "  externs =\n"
        "  rustdeps =\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/source.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  target.private_deps().push_back(LabelTargetPair(&procmacro));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_extension = \n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build ./foo_bar: rust_bin ../../foo/main.rs | ../../foo/source.rs "
        "../../foo/main.rs obj/bar/libmymacro.so\n"
        "  externs = --extern mymacro=obj/bar/libmymacro.so\n"
        "  rustdeps = -Ldependency=obj/bar\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, GroupDeps) {
  Err err;
  TestWithScope setup;

  Target rlib(setup.settings(), Label(SourceDir("//bar/"), "mylib"));
  rlib.set_output_type(Target::RUST_LIBRARY);
  rlib.visibility().SetPublic();
  SourceFile barlib("//bar/lib.rs");
  rlib.sources().push_back(SourceFile("//bar/mylib.rs"));
  rlib.sources().push_back(barlib);
  rlib.source_types_used().Set(SourceFile::SOURCE_RS);
  rlib.rust_values().set_crate_root(barlib);
  rlib.rust_values().crate_name() = "mylib";
  rlib.SetToolchain(setup.toolchain());
  ASSERT_TRUE(rlib.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&rlib, out);
    writer.Run();

    const char expected[] =
        "crate_name = mylib\n"
        "crate_type = rlib\n"
        "output_extension = .rlib\n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/bar\n"
        "target_output_name = libmylib\n"
        "\n"
        "build obj/bar/libmylib.rlib: rust_rlib ../../bar/lib.rs | "
        "../../bar/mylib.rs ../../bar/lib.rs\n"
        "  externs =\n"
        "  rustdeps =\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }

  Target group(setup.settings(), Label(SourceDir("//baz/"), "group"));
  group.set_output_type(Target::GROUP);
  group.visibility().SetPublic();
  group.public_deps().push_back(LabelTargetPair(&rlib));
  group.SetToolchain(setup.toolchain());
  ASSERT_TRUE(group.OnResolved(&err));

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/source.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  target.private_deps().push_back(LabelTargetPair(&group));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_extension = \n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build ./foo_bar: rust_bin ../../foo/main.rs | ../../foo/source.rs "
        "../../foo/main.rs obj/bar/libmylib.rlib || obj/baz/group.stamp\n"
        "  externs = --extern mylib=obj/bar/libmylib.rlib\n"
        "  rustdeps = -Ldependency=obj/bar\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, Externs) {
  Err err;
  TestWithScope setup;

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/source.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  target.config_values().externs().push_back(
      std::pair("lib1", LibFile(SourceFile("//foo/lib1.rlib"))));
  target.config_values().externs().push_back(
      std::pair("lib2", LibFile("lib2.rlib")));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_extension = \n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build ./foo_bar: rust_bin ../../foo/main.rs | ../../foo/source.rs "
        "../../foo/main.rs ../../foo/lib1.rlib\n"
        "  externs = --extern lib1=../../foo/lib1.rlib --extern "
        "lib2=lib2.rlib\n"
        "  rustdeps =\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}
