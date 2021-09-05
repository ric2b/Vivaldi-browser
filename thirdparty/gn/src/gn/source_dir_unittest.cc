// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/source_dir.h"
#include "gn/err.h"
#include "gn/source_file.h"
#include "gn/test_with_scope.h"
#include "gn/value.h"
#include "util/build_config.h"
#include "util/test/test.h"

TEST(SourceDir, ResolveRelativeFile) {
  Err err;
  SourceDir base("//base/");
#if defined(OS_WIN)
  std::string_view source_root("C:/source/root");
#else
  std::string_view source_root("/source/root");
#endif

  // Empty input is an error.
  EXPECT_TRUE(base.ResolveRelativeFile(Value(nullptr, std::string()), &err,
                                       source_root) == SourceFile());
  EXPECT_TRUE(err.has_error());

  // These things are directories, so should be an error.
  err = Err();
  EXPECT_TRUE(base.ResolveRelativeFile(Value(nullptr, "//foo/bar/"), &err,
                                       source_root) == SourceFile());
  EXPECT_TRUE(err.has_error());

  err = Err();
  EXPECT_TRUE(base.ResolveRelativeFile(Value(nullptr, "bar/"), &err,
                                       source_root) == SourceFile());
  EXPECT_TRUE(err.has_error());

  // Absolute paths should be passed unchanged.
  err = Err();
  EXPECT_TRUE(base.ResolveRelativeFile(Value(nullptr, "//foo"), &err,
                                       source_root) == SourceFile("//foo"));
  EXPECT_FALSE(err.has_error());

  EXPECT_TRUE(base.ResolveRelativeFile(Value(nullptr, "/foo"), &err,
                                       source_root) == SourceFile("/foo"));
  EXPECT_FALSE(err.has_error());

  // Basic relative stuff.
  EXPECT_TRUE(
      base.ResolveRelativeFile(Value(nullptr, "foo"), &err, source_root) ==
      SourceFile("//base/foo"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(
      base.ResolveRelativeFile(Value(nullptr, "./foo"), &err, source_root) ==
      SourceFile("//base/foo"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(base.ResolveRelativeFile(Value(nullptr, "../foo"), &err,
                                       source_root) == SourceFile("//foo"));
  EXPECT_FALSE(err.has_error());

// If the given relative path points outside the source root, we
// expect an absolute path.
#if defined(OS_WIN)
  EXPECT_TRUE(base.ResolveRelativeFile(Value(nullptr, "../../foo"), &err,
                                       source_root) ==
              SourceFile("/C:/source/foo"));
  EXPECT_FALSE(err.has_error());

  EXPECT_TRUE(
      base.ResolveRelativeFile(Value(nullptr, "//../foo"), &err, source_root) ==
      SourceFile("/C:/source/foo"));
  EXPECT_FALSE(err.has_error());

  EXPECT_TRUE(base.ResolveRelativeFile(Value(nullptr, "//../root/foo"), &err,
                                       source_root) ==
              SourceFile("/C:/source/root/foo"));
  EXPECT_FALSE(err.has_error());

  EXPECT_TRUE(base.ResolveRelativeFile(Value(nullptr, "//../../../foo/bar"),
                                       &err,
                                       source_root) == SourceFile("/foo/bar"));
  EXPECT_FALSE(err.has_error());
#else
  EXPECT_TRUE(base.ResolveRelativeFile(Value(nullptr, "../../foo"), &err,
                                       source_root) ==
              SourceFile("/source/foo"));
  EXPECT_FALSE(err.has_error());

  EXPECT_TRUE(
      base.ResolveRelativeFile(Value(nullptr, "//../foo"), &err, source_root) ==
      SourceFile("/source/foo"));
  EXPECT_FALSE(err.has_error());

  EXPECT_TRUE(base.ResolveRelativeFile(Value(nullptr, "//../root/foo"), &err,
                                       source_root) ==
              SourceFile("/source/root/foo"));
  EXPECT_FALSE(err.has_error());

  EXPECT_TRUE(base.ResolveRelativeFile(Value(nullptr, "//../../../foo/bar"),
                                       &err,
                                       source_root) == SourceFile("/foo/bar"));
  EXPECT_FALSE(err.has_error());
#endif

#if defined(OS_WIN)
  // Note that we don't canonicalize the backslashes to forward slashes.
  // This could potentially be changed in the future which would mean we should
  // just change the expected result.
  EXPECT_TRUE(base.ResolveRelativeFile(Value(nullptr, "C:\\foo\\bar.txt"), &err,
                                       source_root) ==
              SourceFile("/C:/foo/bar.txt"));
  EXPECT_FALSE(err.has_error());
#endif
}

TEST(SourceDir, ResolveRelativeDir) {
  Err err;
  SourceDir base("//base/");
#if defined(OS_WIN)
  std::string_view source_root("C:/source/root");
#else
  std::string_view source_root("/source/root");
#endif

  // Empty input is an error.
  EXPECT_TRUE(base.ResolveRelativeDir(Value(nullptr, std::string()), &err,
                                      source_root) == SourceDir());
  EXPECT_TRUE(err.has_error());

  // Absolute paths should be passed unchanged.
  err = Err();
  EXPECT_TRUE(base.ResolveRelativeDir(Value(nullptr, "//foo"), &err,
                                      source_root) == SourceDir("//foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(base.ResolveRelativeDir(Value(nullptr, "/foo"), &err,
                                      source_root) == SourceDir("/foo/"));
  EXPECT_FALSE(err.has_error());

  // Basic relative stuff.
  EXPECT_TRUE(base.ResolveRelativeDir(Value(nullptr, "foo"), &err,
                                      source_root) == SourceDir("//base/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(base.ResolveRelativeDir(Value(nullptr, "./foo"), &err,
                                      source_root) == SourceDir("//base/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(base.ResolveRelativeDir(Value(nullptr, "../foo"), &err,
                                      source_root) == SourceDir("//foo/"));
  EXPECT_FALSE(err.has_error());

// If the given relative path points outside the source root, we
// expect an absolute path.
#if defined(OS_WIN)
  EXPECT_TRUE(
      base.ResolveRelativeDir(Value(nullptr, "../../foo"), &err, source_root) ==
      SourceDir("/C:/source/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(
      base.ResolveRelativeDir(Value(nullptr, "//../foo"), &err, source_root) ==
      SourceDir("/C:/source/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(base.ResolveRelativeDir(Value(nullptr, "//.."), &err,
                                      source_root) == SourceDir("/C:/source/"));
  EXPECT_FALSE(err.has_error());
#else
  EXPECT_TRUE(
      base.ResolveRelativeDir(Value(nullptr, "../../foo"), &err, source_root) ==
      SourceDir("/source/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(
      base.ResolveRelativeDir(Value(nullptr, "//../foo"), &err, source_root) ==
      SourceDir("/source/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(base.ResolveRelativeDir(Value(nullptr, "//.."), &err,
                                      source_root) == SourceDir("/source/"));
  EXPECT_FALSE(err.has_error());
#endif

#if defined(OS_WIN)
  // Canonicalize the existing backslashes to forward slashes and add a
  // leading slash if necessary.
  EXPECT_TRUE(base.ResolveRelativeDir(Value(nullptr, "\\C:\\foo"), &err) ==
              SourceDir("/C:/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(base.ResolveRelativeDir(Value(nullptr, "C:\\foo"), &err) ==
              SourceDir("/C:/foo/"));
  EXPECT_FALSE(err.has_error());
#endif
}

TEST(SourceDir, SourceWithNoTrailingSlash) {
  Err err;
  SourceDir base("//base/");
  SourceDir base_no_slash("//base/");
  EXPECT_EQ(base.SourceWithNoTrailingSlash(), "//base");
  EXPECT_EQ(base_no_slash.SourceWithNoTrailingSlash(), "//base");

  SourceDir relative_root("//");
  EXPECT_EQ(relative_root.SourceWithNoTrailingSlash(), "//");

#if defined(OS_WIN)
  SourceDir root("C:/");
  SourceDir root_no_slash("C:");
  EXPECT_EQ(root.SourceWithNoTrailingSlash(), "C:");
  EXPECT_EQ(root_no_slash.SourceWithNoTrailingSlash(), "C:");
#else
  SourceDir root("/");
  EXPECT_EQ(root.SourceWithNoTrailingSlash(), "/");
#endif
}

class TestWithAlias : public TestWithScope {
 public:
  TestWithAlias() {
    // Labels for top directory of source is prefixed //alpha
    // Subdirectory "gamma" in top directory is the codebase using //foo labels
    // This means that the relative path from top directory for //foo is
    // "gamma/foo" - except //alpha and //beta
    // Subdirectory "beta" in top directory maps to //beta (not //gamma/beta)
    build_settings()->RegisterPathMap("//alpha", "//");
    build_settings()->RegisterPathMap("//beta", "//beta");
    build_settings()->RegisterPathMap("//", "//gamma");
  }
};

TEST(SourceDir, Alias_MapToActual) {
  TestWithAlias scope;

  SourceDir a("//alpha/a/b/c");
  SourceDir b("//beta/a/b/c");
  SourceDir c("//foo/a/b/c");

  ASSERT_EQ(a.actual_path(), "//a/b/c/");
  ASSERT_EQ(b.actual_path(), "//beta/a/b/c/");
  ASSERT_EQ(c.actual_path(), "//gamma/foo/a/b/c/");
}

TEST(SourceDir, Alias_MapFromActual) {
  TestWithAlias scope;

  SourceDir a("//alpha/a/b/c");
  SourceDir b("//beta/a/b/c");
  SourceDir c("//foo/a/b/c");

  ASSERT_EQ(a.value(), BuildSettings::RemapActualToSourcePath("//a/b/c/"));
  ASSERT_EQ(b.value(), BuildSettings::RemapActualToSourcePath("//beta/a/b/c/"));
  ASSERT_EQ(c.value(),
            BuildSettings::RemapActualToSourcePath("//gamma/foo/a/b/c/"));
}

TEST(SourceDir, Alias_RelativePath) {
  TestWithAlias scope;

  Err err;
  SourceDir alpha("//alpha/");
  SourceDir beta("//beta/");
  SourceDir delta("//delta/");

#if defined(OS_WIN)
  std::string_view source_root("C:/source/root");
#else
  std::string_view source_root("/source/root");
#endif

  // Basic relative stuff.
  EXPECT_TRUE(
      alpha.ResolveRelativeDir(Value(nullptr, "foo"), &err, source_root) ==
      SourceDir("//alpha/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(
      alpha.ResolveRelativeDir(Value(nullptr, "./foo"), &err, source_root) ==
      SourceDir("//alpha/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeDir(Value(nullptr, "gamma/foo"), &err,
                                       source_root) == SourceDir("//foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeDir(Value(nullptr, "./gamma/foo"), &err,
                                       source_root) == SourceDir("//foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeDir(Value(nullptr, "//alpha/foo"), &err,
                                       source_root) ==
              SourceDir("//alpha/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeDir(Value(nullptr, "//beta/foo"), &err,
                                       source_root) ==
              SourceDir("//beta/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeDir(Value(nullptr, "//delta/foo"), &err,
                                       source_root) ==
              SourceDir("//delta/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeDir(Value(nullptr, "foo"), &err,
                                      source_root) == SourceDir("//beta/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeDir(Value(nullptr, "./foo"), &err,
                                      source_root) == SourceDir("//beta/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeDir(Value(nullptr, "../foo"), &err,
                                      source_root) == SourceDir("//alpha/foo"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeDir(Value(nullptr, "//alpha/foo"), &err,
                                      source_root) ==
              SourceDir("//alpha/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeDir(Value(nullptr, "//beta/foo"), &err,
                                      source_root) == SourceDir("//beta/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeDir(Value(nullptr, "//delta/foo"), &err,
                                      source_root) ==
              SourceDir("//delta/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(
      delta.ResolveRelativeDir(Value(nullptr, "foo"), &err, source_root) ==
      SourceDir("//delta/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(
      delta.ResolveRelativeDir(Value(nullptr, "./foo"), &err, source_root) ==
      SourceDir("//delta/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeDir(Value(nullptr, "../foo"), &err,
                                       source_root) == SourceDir("//foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeDir(Value(nullptr, "../../foo"), &err,
                                       source_root) ==
              SourceDir("//alpha/foo"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeDir(Value(nullptr, "//alpha/foo"), &err,
                                       source_root) ==
              SourceDir("//alpha/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeDir(Value(nullptr, "//beta/foo"), &err,
                                       source_root) ==
              SourceDir("//beta/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeDir(Value(nullptr, "//delta/foo"), &err,
                                       source_root) ==
              SourceDir("//delta/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeAs(false, Value(nullptr, "foo"), &err,
                                      source_root) == "//alpha/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeAs(false, Value(nullptr, "./foo"), &err,
                                      source_root) == "//alpha/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeAs(false, Value(nullptr, "gamma/foo"), &err,
                                      source_root) == "//foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeAs(false, Value(nullptr, "./gamma/foo"),
                                      &err, source_root) == "//foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeAs(false, Value(nullptr, "//alpha/foo"),
                                      &err, source_root) == "//alpha/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeAs(false, Value(nullptr, "//beta/foo"), &err,
                                      source_root) == "//beta/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeAs(false, Value(nullptr, "//delta/foo"),
                                      &err, source_root) == "//delta/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeAs(false, Value(nullptr, "foo"), &err,
                                     source_root) == "//beta/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeAs(false, Value(nullptr, "./foo"), &err,
                                     source_root) == "//beta/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeAs(false, Value(nullptr, "../foo"), &err,
                                     source_root) == "//alpha/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeAs(false, Value(nullptr, "//alpha/foo"), &err,
                                     source_root) == "//alpha/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeAs(false, Value(nullptr, "//beta/foo"), &err,
                                     source_root) == "//beta/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeAs(false, Value(nullptr, "//delta/foo"), &err,
                                     source_root) == "//delta/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeAs(false, Value(nullptr, "foo"), &err,
                                      source_root) == "//delta/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeAs(false, Value(nullptr, "./foo"), &err,
                                      source_root) == "//delta/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeAs(false, Value(nullptr, "../foo"), &err,
                                      source_root) == "//foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeAs(false, Value(nullptr, "../../foo/"), &err,
                                      source_root) == "//alpha/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeAs(false, Value(nullptr, "//alpha/foo"),
                                      &err, source_root) == "//alpha/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeAs(false, Value(nullptr, "//beta/foo"), &err,
                                      source_root) == "//beta/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeAs(false, Value(nullptr, "//delta/foo"),
                                      &err, source_root) == "//delta/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeAs(true, Value(nullptr, "foo"), &err,
                                      source_root) == "//alpha/foo");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeAs(true, Value(nullptr, "./foo"), &err,
                                      source_root) == "//alpha/foo");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeAs(true, Value(nullptr, "gamma/foo"), &err,
                                      source_root) == "//foo");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeAs(true, Value(nullptr, "./gamma/foo"), &err,
                                      source_root) == "//foo");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeAs(true, Value(nullptr, "foo"), &err,
                                      source_root) == "//alpha/foo");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeAs(true, Value(nullptr, "./foo"), &err,
                                      source_root) == "//alpha/foo");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeAs(true, Value(nullptr, "gamma/foo"), &err,
                                      source_root) == "//foo");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeAs(true, Value(nullptr, "foo"), &err,
                                     source_root) == "//beta/foo");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeAs(true, Value(nullptr, "./foo"), &err,
                                     source_root) == "//beta/foo");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeAs(true, Value(nullptr, "../foo"), &err,
                                     source_root) == "//alpha/foo");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeAs(true, Value(nullptr, "//alpha/foo"), &err,
                                     source_root) == "//alpha/foo");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeAs(true, Value(nullptr, "//beta/foo"), &err,
                                     source_root) == "//beta/foo");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeAs(true, Value(nullptr, "//delta/foo"), &err,
                                     source_root) == "//delta/foo");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeAs(true, Value(nullptr, "foo"), &err,
                                      source_root) == "//delta/foo");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeAs(true, Value(nullptr, "./foo"), &err,
                                      source_root) == "//delta/foo");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeAs(true, Value(nullptr, "../foo"), &err,
                                      source_root) == "//foo");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeAs(true, Value(nullptr, "../../foo"), &err,
                                      source_root) == "//alpha/foo");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeAs(true, Value(nullptr, "//alpha/foo"), &err,
                                      source_root) == "//alpha/foo");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeAs(true, Value(nullptr, "//beta/foo"), &err,
                                      source_root) == "//beta/foo");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeAs(true, Value(nullptr, "//delta/foo"), &err,
                                      source_root) == "//delta/foo");
  EXPECT_FALSE(err.has_error());

  // If the given relative path points outside the source root, we
  // expect an absolute path.
#if defined(OS_WIN)
  EXPECT_TRUE(
      alpha.ResolveRelativeDir(Value(nullptr, "../foo"), &err, source_root) ==
      SourceDir("/C:/source/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeDir(Value(nullptr, "//../../foo"), &err,
                                       source_root) ==
              SourceDir("/C:/source/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(
      alpha.ResolveRelativeDir(Value(nullptr, "//../.."), &err, source_root) ==
      SourceDir("/C:/source/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(
      beta.ResolveRelativeDir(Value(nullptr, "../../foo"), &err, source_root) ==
      SourceDir("/C:/source/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeDir(Value(nullptr, "//../../foo"), &err,
                                      source_root) ==
              SourceDir("/C:/source/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeDir(Value(nullptr, "//../.."), &err,
                                      source_root) == SourceDir("/C:/source/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeDir(Value(nullptr, "../../../foo"), &err,
                                       source_root) ==
              SourceDir("/C:/source/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeDir(Value(nullptr, "//../../foo"), &err,
                                       source_root) ==
              SourceDir("/C:/source/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(
      delta.ResolveRelativeDir(Value(nullptr, "//../.."), &err, source_root) ==
      SourceDir("/C:/source/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeAs(false, Value(nullptr, "../foo"), &err,
                                      source_root) == "/C:/source/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeAs(false, Value(nullptr, "//../../foo"),
                                      &err, source_root) == "/C:/source/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeAs(false, Value(nullptr, "//../.."), &err,
                                      source_root) == "/C:/source/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeAs(false, Value(nullptr, "../../foo"), &err,
                                     source_root) == "/C:/source/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeAs(false, Value(nullptr, "//../../foo"), &err,
                                     source_root) == "/C:/source/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeAs(false, Value(nullptr, "//../.."), &err,
                                     source_root) == "/C:/source/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeAs(false, Value(nullptr, "../../../foo"),
                                      &err, source_root) == "/C:/source/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeAs(false, Value(nullptr, "//../../foo"),
                                      &err, source_root) == "/C:/source/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeAs(false, Value(nullptr, "//../.."), &err,
                                      source_root) == "/C:/source/");
  EXPECT_FALSE(err.has_error());
#else
  EXPECT_TRUE(
      alpha.ResolveRelativeDir(Value(nullptr, "../foo"), &err, source_root) ==
      SourceDir("/source/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeDir(Value(nullptr, "//../../foo"), &err,
                                       source_root) ==
              SourceDir("/source/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeDir(Value(nullptr, "//../.."), &err,
                                       source_root) == SourceDir("/source/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(
      beta.ResolveRelativeDir(Value(nullptr, "../../foo"), &err, source_root) ==
      SourceDir("/source/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeDir(Value(nullptr, "//../../foo"), &err,
                                      source_root) ==
              SourceDir("/source/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeDir(Value(nullptr, "//../.."), &err,
                                      source_root) == SourceDir("/source/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeDir(Value(nullptr, "../../../foo"), &err,
                                       source_root) ==
              SourceDir("/source/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeDir(Value(nullptr, "//../../foo"), &err,
                                       source_root) ==
              SourceDir("/source/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeDir(Value(nullptr, "//../.."), &err,
                                       source_root) == SourceDir("/source/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeAs(false, Value(nullptr, "../foo"), &err,
                                      source_root) == "/source/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeAs(false, Value(nullptr, "//../../foo"),
                                      &err, source_root) == "/source/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeAs(false, Value(nullptr, "//../.."), &err,
                                      source_root) == "/source/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeAs(false, Value(nullptr, "../../foo"), &err,
                                     source_root) == "/source/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeAs(false, Value(nullptr, "//../../foo"), &err,
                                     source_root) == "/source/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(beta.ResolveRelativeAs(false, Value(nullptr, "//../.."), &err,
                                     source_root) == "/source/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeAs(false, Value(nullptr, "../../../foo"),
                                      &err, source_root) == "/source/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeAs(false, Value(nullptr, "//../../foo"),
                                      &err, source_root) == "/source/foo/");
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(delta.ResolveRelativeAs(false, Value(nullptr, "//../.."), &err,
                                      source_root) == "/source/");
  EXPECT_FALSE(err.has_error());
#endif

#if defined(OS_WIN)
  // Canonicalize the existing backslashes to forward slashes and add a
  // leading slash if necessary.
  EXPECT_TRUE(alpha.ResolveRelativeDir(Value(nullptr, "\\C:\\foo"), &err) ==
              SourceDir("/C:/foo/"));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(alpha.ResolveRelativeDir(Value(nullptr, "C:\\foo"), &err) ==
              SourceDir("/C:/foo/"));
  EXPECT_FALSE(err.has_error());
#endif
}

TEST(SourceDir, Alias_DirFullpath) {
  TestWithAlias scope;

#if defined(OS_WIN)
  base::FilePath::StringType root_path(FILE_PATH_LITERAL("C:/Source/root"));
#else
  std::string_view root_path(FILE_PATH_LITERAL("/Source/root"));
#endif
  base::FilePath root(root_path);
  scope.build_settings()->SetRootPath(root);

  base::FilePath a(
      scope.build_settings()->GetFullPath("//alpha/a/b/c/", false));
  base::FilePath b(scope.build_settings()->GetFullPath("//beta/a/b/c/", false));
  base::FilePath c(scope.build_settings()->GetFullPath("//foo/a/b/c/", false));
  base::FilePath d(
      scope.build_settings()->GetFullPath("//alpha/gamma/foo/a/b/c/", false));

  ASSERT_EQ(a.value(), root_path + FILE_PATH_LITERAL("/a/b/c/"));
  ASSERT_EQ(b.value(), root_path + FILE_PATH_LITERAL("/beta/a/b/c/"));
  ASSERT_EQ(c.value(), root_path + FILE_PATH_LITERAL("/gamma/foo/a/b/c/"));
  ASSERT_EQ(d.value(), root_path + FILE_PATH_LITERAL("/gamma/foo/a/b/c/"));
}

TEST(SourceDir, Alias_FileFullpath) {
  TestWithAlias scope;

#if defined(OS_WIN)
  base::FilePath::StringType root_path(FILE_PATH_LITERAL("C:/Source/root"));
#else
  std::string_view root_path(FILE_PATH_LITERAL("/Source/root"));
#endif
  base::FilePath root(root_path);
  scope.build_settings()->SetRootPath(root);

  base::FilePath a(scope.build_settings()->GetFullPath("//alpha/a/b/c", true));
  base::FilePath b(scope.build_settings()->GetFullPath("//beta/a/b/c", true));
  base::FilePath c(scope.build_settings()->GetFullPath("//foo/a/b/c", true));
  base::FilePath d(
      scope.build_settings()->GetFullPath("//alpha/gamma/foo/a/b/c", true));

  ASSERT_EQ(a.value(), root_path + FILE_PATH_LITERAL("/a/b/c"));
  ASSERT_EQ(b.value(), root_path + FILE_PATH_LITERAL("/beta/a/b/c"));
  ASSERT_EQ(c.value(), root_path + FILE_PATH_LITERAL("/gamma/foo/a/b/c"));
  ASSERT_EQ(d.value(), root_path + FILE_PATH_LITERAL("/gamma/foo/a/b/c"));
}

TEST(SourceDir, Alias_SourceDirFullpath) {
  TestWithAlias scope;

#if defined(OS_WIN)
  base::FilePath::StringType root_path(FILE_PATH_LITERAL("C:/Source/root"));
#else
  std::string_view root_path(FILE_PATH_LITERAL("/Source/root"));
#endif
  base::FilePath root(root_path);
  scope.build_settings()->SetRootPath(root);

  SourceDir alpha("//alpha/a/b/c/");
  SourceDir beta("//beta/a/b/c/");
  SourceDir foo("//foo/a/b/c/");
  SourceDir gamma("//alpha/gamma/foo/a/b/c/");

  base::FilePath a(scope.build_settings()->GetFullPath(alpha));
  base::FilePath b(scope.build_settings()->GetFullPath(beta));
  base::FilePath c(scope.build_settings()->GetFullPath(foo));
  base::FilePath d(scope.build_settings()->GetFullPath(gamma));

  ASSERT_EQ(a.value(), root_path + FILE_PATH_LITERAL("/a/b/c/"));
  ASSERT_EQ(b.value(), root_path + FILE_PATH_LITERAL("/beta/a/b/c/"));
  ASSERT_EQ(c.value(), root_path + FILE_PATH_LITERAL("/gamma/foo/a/b/c/"));
  ASSERT_EQ(d.value(), root_path + FILE_PATH_LITERAL("/gamma/foo/a/b/c/"));
}

TEST(SourceDir, Alias_SourceFileFullpath) {
  TestWithAlias scope;

#if defined(OS_WIN)
  base::FilePath::StringType root_path(FILE_PATH_LITERAL("C:/Source/root"));
#else
  std::string_view root_path(FILE_PATH_LITERAL("/Source/root"));
#endif
  base::FilePath root(root_path);
  scope.build_settings()->SetRootPath(root);

  SourceFile alpha("//alpha/a/b/c");
  SourceFile beta("//beta/a/b/c");
  SourceFile foo("//foo/a/b/c");
  SourceFile gamma("//alpha/gamma/foo/a/b/c");

  base::FilePath a(scope.build_settings()->GetFullPath(alpha));
  base::FilePath b(scope.build_settings()->GetFullPath(beta));
  base::FilePath c(scope.build_settings()->GetFullPath(foo));
  base::FilePath d(scope.build_settings()->GetFullPath(gamma));

  ASSERT_EQ(a.value(), root_path + FILE_PATH_LITERAL("/a/b/c"));
  ASSERT_EQ(b.value(), root_path + FILE_PATH_LITERAL("/beta/a/b/c"));
  ASSERT_EQ(c.value(), root_path + FILE_PATH_LITERAL("/gamma/foo/a/b/c"));
  ASSERT_EQ(d.value(), root_path + FILE_PATH_LITERAL("/gamma/foo/a/b/c"));
}
