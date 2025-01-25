// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/ninja_create_bundle_target_writer.h"

#include <algorithm>
#include <memory>
#include <sstream>

#include "gn/target.h"
#include "gn/test_with_scope.h"
#include "util/test/test.h"

namespace {

void SetupBundleDataDir(BundleData* bundle_data, const std::string& root_dir) {
  std::string bundle_root_dir = root_dir + "/bar.bundle";
  bundle_data->root_dir() = SourceDir(bundle_root_dir);
  bundle_data->contents_dir() = SourceDir(bundle_root_dir + "/Contents");
  bundle_data->resources_dir() =
      SourceDir(bundle_data->contents_dir().value() + "/Resources");
  bundle_data->executable_dir() =
      SourceDir(bundle_data->contents_dir().value() + "/MacOS");
}

std::unique_ptr<Target> NewAction(const TestWithScope& setup) {
  Err err;
  auto action = std::make_unique<Target>(setup.settings(),
                                         Label(SourceDir("//foo/"), "bar"));
  action->set_output_type(Target::ACTION);
  action->visibility().SetPublic();
  action->action_values().set_script(SourceFile("//foo/script.py"));

  action->action_values().outputs() =
      SubstitutionList::MakeForTest("//out/Debug/foo.out");

  action->SetToolchain(setup.toolchain());
  return action;
}

}  // namespace

// Tests multiple files with an output pattern.
TEST(NinjaCreateBundleTargetWriter, Run) {
  Err err;
  TestWithScope setup;

  std::unique_ptr<Target> action = NewAction(setup);
  ASSERT_TRUE(action->OnResolved(&err)) << err.message();

  Target bundle_data(setup.settings(), Label(SourceDir("//foo/"), "data"));
  bundle_data.set_output_type(Target::BUNDLE_DATA);
  bundle_data.sources().push_back(SourceFile("//foo/input1.txt"));
  bundle_data.sources().push_back(SourceFile("//foo/input2.txt"));
  bundle_data.action_values().outputs() = SubstitutionList::MakeForTest(
      "{{bundle_resources_dir}}/{{source_file_part}}");
  bundle_data.SetToolchain(setup.toolchain());
  bundle_data.visibility().SetPublic();
  ASSERT_TRUE(bundle_data.OnResolved(&err));

  Target create_bundle(
      setup.settings(),
      Label(SourceDir("//baz/"), "bar", setup.toolchain()->label().dir(),
            setup.toolchain()->label().name()));
  SetupBundleDataDir(&create_bundle.bundle_data(), "//out/Debug");
  create_bundle.set_output_type(Target::CREATE_BUNDLE);
  create_bundle.private_deps().push_back(LabelTargetPair(&bundle_data));
  create_bundle.private_deps().push_back(LabelTargetPair(action.get()));
  create_bundle.SetToolchain(setup.toolchain());
  ASSERT_TRUE(create_bundle.OnResolved(&err));

  std::ostringstream out;
  NinjaCreateBundleTargetWriter writer(&create_bundle, out);
  writer.Run();

  const char expected[] =
      "build phony/baz/bar.inputdeps: phony phony/foo/bar "
      "phony/foo/data\n"
      "build bar.bundle/Contents/Resources/input1.txt: copy_bundle_data "
      "../../foo/input1.txt || phony/baz/bar.inputdeps\n"
      "build bar.bundle/Contents/Resources/input2.txt: copy_bundle_data "
      "../../foo/input2.txt || phony/baz/bar.inputdeps\n"
      "build phony/baz/bar: phony "
      "bar.bundle/Contents/Resources/input1.txt "
      "bar.bundle/Contents/Resources/input2.txt"
      " || phony/baz/bar.inputdeps\n"
      "build bar.bundle: phony phony/baz/bar\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected, out_str);
}

// Tests creating a bundle in a sub-directory of $root_out_dir.
TEST(NinjaCreateBundleTargetWriter, InSubDirectory) {
  Err err;
  TestWithScope setup;

  std::unique_ptr<Target> action = NewAction(setup);
  ASSERT_TRUE(action->OnResolved(&err)) << err.message();

  Target bundle_data(setup.settings(), Label(SourceDir("//foo/"), "data"));
  bundle_data.set_output_type(Target::BUNDLE_DATA);
  bundle_data.sources().push_back(SourceFile("//foo/input1.txt"));
  bundle_data.sources().push_back(SourceFile("//foo/input2.txt"));
  bundle_data.action_values().outputs() = SubstitutionList::MakeForTest(
      "{{bundle_resources_dir}}/{{source_file_part}}");
  bundle_data.SetToolchain(setup.toolchain());
  bundle_data.visibility().SetPublic();
  ASSERT_TRUE(bundle_data.OnResolved(&err));

  Target create_bundle(
      setup.settings(),
      Label(SourceDir("//baz/"), "bar", setup.toolchain()->label().dir(),
            setup.toolchain()->label().name()));
  SetupBundleDataDir(&create_bundle.bundle_data(), "//out/Debug/gen");
  create_bundle.set_output_type(Target::CREATE_BUNDLE);
  create_bundle.private_deps().push_back(LabelTargetPair(&bundle_data));
  create_bundle.private_deps().push_back(LabelTargetPair(action.get()));
  create_bundle.SetToolchain(setup.toolchain());
  ASSERT_TRUE(create_bundle.OnResolved(&err));

  std::ostringstream out;
  NinjaCreateBundleTargetWriter writer(&create_bundle, out);
  writer.Run();

  const char expected[] =
      "build phony/baz/bar.inputdeps: phony phony/foo/bar "
      "phony/foo/data\n"
      "build gen/bar.bundle/Contents/Resources/input1.txt: copy_bundle_data "
      "../../foo/input1.txt || phony/baz/bar.inputdeps\n"
      "build gen/bar.bundle/Contents/Resources/input2.txt: copy_bundle_data "
      "../../foo/input2.txt || phony/baz/bar.inputdeps\n"
      "build phony/baz/bar: phony "
      "gen/bar.bundle/Contents/Resources/input1.txt "
      "gen/bar.bundle/Contents/Resources/input2.txt || "
      "phony/baz/bar.inputdeps\n"
      "build gen/bar.bundle: phony phony/baz/bar\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected, out_str);
}

// Tests empty asset catalog with partial_info_plist property defined.
TEST(NinjaCreateBundleTargetWriter, JustPartialInfoPlist) {
  Err err;
  TestWithScope setup;

  std::unique_ptr<Target> action = NewAction(setup);
  ASSERT_TRUE(action->OnResolved(&err)) << err.message();

  Target create_bundle(
      setup.settings(),
      Label(SourceDir("//baz/"), "bar", setup.toolchain()->label().dir(),
            setup.toolchain()->label().name()));
  SetupBundleDataDir(&create_bundle.bundle_data(), "//out/Debug");
  create_bundle.set_output_type(Target::CREATE_BUNDLE);
  create_bundle.private_deps().push_back(LabelTargetPair(action.get()));
  create_bundle.bundle_data().product_type().assign("com.apple.product-type");
  create_bundle.bundle_data().set_partial_info_plist(
      SourceFile("//out/Debug/baz/bar/bar_partial_info.plist"));
  create_bundle.SetToolchain(setup.toolchain());
  ASSERT_TRUE(create_bundle.OnResolved(&err));

  std::ostringstream out;
  NinjaCreateBundleTargetWriter writer(&create_bundle, out);
  writer.Run();

  const char expected[] =
      "build baz/bar/bar_partial_info.plist: stamp || phony/foo/bar\n"
      "build phony/baz/bar: phony "
      "baz/bar/bar_partial_info.plist || phony/foo/bar\n"
      "build bar.bundle: phony phony/baz/bar\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
}

// Tests multiple files from asset catalog.
TEST(NinjaCreateBundleTargetWriter, AssetCatalog) {
  Err err;
  TestWithScope setup;

  std::unique_ptr<Target> action = NewAction(setup);
  ASSERT_TRUE(action->OnResolved(&err)) << err.message();

  Target bundle_data(setup.settings(), Label(SourceDir("//foo/"), "data"));
  bundle_data.set_output_type(Target::BUNDLE_DATA);
  bundle_data.sources().push_back(
      SourceFile("//foo/Foo.xcassets/Contents.json"));
  bundle_data.sources().push_back(
      SourceFile("//foo/Foo.xcassets/foo.colorset/Contents.json"));
  bundle_data.sources().push_back(
      SourceFile("//foo/Foo.xcassets/foo.imageset/Contents.json"));
  bundle_data.sources().push_back(
      SourceFile("//foo/Foo.xcassets/foo.imageset/FooIcon-29.png"));
  bundle_data.sources().push_back(
      SourceFile("//foo/Foo.xcassets/foo.imageset/FooIcon-29@2x.png"));
  bundle_data.sources().push_back(
      SourceFile("//foo/Foo.xcassets/foo.imageset/FooIcon-29@3x.png"));
  bundle_data.sources().push_back(
      SourceFile("//foo/Foo.xcassets/foo.dataset/Contents.json"));
  bundle_data.sources().push_back(
      SourceFile("//foo/Foo.xcassets/foo.dataset/FooScript.js"));
  bundle_data.sources().push_back(
      SourceFile("//foo/Foo.xcassets/file/with/no/known/pattern"));
  bundle_data.sources().push_back(
      SourceFile("//foo/Foo.xcassets/nested/bar.xcassets/my/file"));
  bundle_data.action_values().outputs() = SubstitutionList::MakeForTest(
      "{{bundle_resources_dir}}/{{source_file_part}}");
  bundle_data.SetToolchain(setup.toolchain());
  bundle_data.visibility().SetPublic();
  ASSERT_TRUE(bundle_data.OnResolved(&err));

  Target create_bundle(
      setup.settings(),
      Label(SourceDir("//baz/"), "bar", setup.toolchain()->label().dir(),
            setup.toolchain()->label().name()));
  SetupBundleDataDir(&create_bundle.bundle_data(), "//out/Debug");
  create_bundle.set_output_type(Target::CREATE_BUNDLE);
  create_bundle.private_deps().push_back(LabelTargetPair(&bundle_data));
  create_bundle.private_deps().push_back(LabelTargetPair(action.get()));
  create_bundle.bundle_data().product_type().assign("com.apple.product-type");
  create_bundle.bundle_data().xcasset_compiler_flags() =
      SubstitutionList::MakeForTest("--app-icon", "foo");

  create_bundle.SetToolchain(setup.toolchain());
  ASSERT_TRUE(create_bundle.OnResolved(&err));

  std::ostringstream out;
  NinjaCreateBundleTargetWriter writer(&create_bundle, out);
  writer.Run();

  const char expected[] =
      "build phony/baz/bar.inputdeps: phony phony/foo/bar "
      "phony/foo/data\n"
      "build bar.bundle/Contents/Resources/Assets.car: compile_xcassets "
      "../../foo/Foo.xcassets | phony/foo/data || "
      "phony/baz/bar.inputdeps\n"
      "  product_type = com.apple.product-type\n"
      "  xcasset_compiler_flags = --app-icon foo\n"
      "build phony/baz/bar: phony "
      "bar.bundle/Contents/Resources/Assets.car || "
      "phony/baz/bar.inputdeps\n"
      "build bar.bundle: phony phony/baz/bar\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected, out_str);
}

// Tests that the phony target for the top-level bundle directory is generated
// correctly.
TEST(NinjaCreateBundleTargetWriter, PhonyTarget) {
  Err err;
  TestWithScope setup;

  // An action for our library to depend on.
  Target action(setup.settings(), Label(SourceDir("//foo/"), "action"));
  action.set_output_type(Target::ACTION_FOREACH);
  action.visibility().SetPublic();
  action.SetToolchain(setup.toolchain());
  ASSERT_TRUE(action.OnResolved(&err));

  Target create_bundle(
      setup.settings(),
      Label(SourceDir("//baz/"), "bar", setup.toolchain()->label().dir(),
            setup.toolchain()->label().name()));
  SetupBundleDataDir(&create_bundle.bundle_data(), "//out/Debug");
  create_bundle.set_output_type(Target::CREATE_BUNDLE);
  create_bundle.private_deps().push_back(LabelTargetPair(&action));
  create_bundle.SetToolchain(setup.toolchain());

  ASSERT_TRUE(create_bundle.OnResolved(&err));

  std::ostringstream out;
  NinjaCreateBundleTargetWriter writer(&create_bundle, out);
  writer.Run();

  const char expected[] =
      "build phony/baz/bar: phony || phony/foo/action\n"
      "build bar.bundle: phony phony/baz/bar\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
}

// Tests complex target with multiple bundle_data sources, including
// some asset catalog.
TEST(NinjaCreateBundleTargetWriter, Complex) {
  Err err;
  TestWithScope setup;

  std::unique_ptr<Target> action = NewAction(setup);
  ASSERT_TRUE(action->OnResolved(&err)) << err.message();

  Target bundle_data0(setup.settings(),
                      Label(SourceDir("//qux/"), "info_plist"));
  bundle_data0.set_output_type(Target::BUNDLE_DATA);
  bundle_data0.sources().push_back(SourceFile("//qux/qux-Info.plist"));
  bundle_data0.action_values().outputs() =
      SubstitutionList::MakeForTest("{{bundle_contents_dir}}/Info.plist");
  bundle_data0.SetToolchain(setup.toolchain());
  bundle_data0.visibility().SetPublic();
  ASSERT_TRUE(bundle_data0.OnResolved(&err));

  Target bundle_data1(setup.settings(), Label(SourceDir("//foo/"), "data"));
  bundle_data1.set_output_type(Target::BUNDLE_DATA);
  bundle_data1.sources().push_back(SourceFile("//foo/input1.txt"));
  bundle_data1.sources().push_back(SourceFile("//foo/input2.txt"));
  bundle_data1.action_values().outputs() = SubstitutionList::MakeForTest(
      "{{bundle_resources_dir}}/{{source_file_part}}");
  bundle_data1.SetToolchain(setup.toolchain());
  bundle_data1.visibility().SetPublic();
  ASSERT_TRUE(bundle_data1.OnResolved(&err));

  Target bundle_data2(setup.settings(), Label(SourceDir("//foo/"), "assets"));
  bundle_data2.set_output_type(Target::BUNDLE_DATA);
  bundle_data2.sources().push_back(
      SourceFile("//foo/Foo.xcassets/Contents.json"));
  bundle_data2.sources().push_back(
      SourceFile("//foo/Foo.xcassets/foo.colorset/Contents.json"));
  bundle_data2.sources().push_back(
      SourceFile("//foo/Foo.xcassets/foo.imageset/Contents.json"));
  bundle_data2.sources().push_back(
      SourceFile("//foo/Foo.xcassets/foo.imageset/FooIcon-29.png"));
  bundle_data2.sources().push_back(
      SourceFile("//foo/Foo.xcassets/foo.imageset/FooIcon-29@2x.png"));
  bundle_data2.sources().push_back(
      SourceFile("//foo/Foo.xcassets/foo.imageset/FooIcon-29@3x.png"));
  bundle_data2.sources().push_back(
      SourceFile("//foo/Foo.xcassets/foo.dataset/Contents.json"));
  bundle_data2.sources().push_back(
      SourceFile("//foo/Foo.xcassets/foo.dataset/FooScript.js"));
  bundle_data2.sources().push_back(
      SourceFile("//foo/Foo.xcassets/file/with/no/known/pattern"));
  bundle_data2.sources().push_back(
      SourceFile("//foo/Foo.xcassets/nested/bar.xcassets/my/file"));
  bundle_data2.action_values().outputs() = SubstitutionList::MakeForTest(
      "{{bundle_resources_dir}}/{{source_file_part}}");
  bundle_data2.SetToolchain(setup.toolchain());
  bundle_data2.visibility().SetPublic();
  ASSERT_TRUE(bundle_data2.OnResolved(&err));

  Target bundle_data3(setup.settings(), Label(SourceDir("//quz/"), "assets"));
  bundle_data3.set_output_type(Target::BUNDLE_DATA);
  bundle_data3.sources().push_back(
      SourceFile("//quz/Quz.xcassets/Contents.json"));
  bundle_data3.sources().push_back(
      SourceFile("//quz/Quz.xcassets/quz.imageset/Contents.json"));
  bundle_data3.sources().push_back(
      SourceFile("//quz/Quz.xcassets/quz.imageset/QuzIcon-29.png"));
  bundle_data3.sources().push_back(
      SourceFile("//quz/Quz.xcassets/quz.imageset/QuzIcon-29@2x.png"));
  bundle_data3.sources().push_back(
      SourceFile("//quz/Quz.xcassets/quz.imageset/QuzIcon-29@3x.png"));
  bundle_data3.sources().push_back(
      SourceFile("//quz/Quz.xcassets/quz.dataset/Contents.json"));
  bundle_data3.sources().push_back(
      SourceFile("//quz/Quz.xcassets/quz.dataset/QuzScript.js"));
  bundle_data3.action_values().outputs() = SubstitutionList::MakeForTest(
      "{{bundle_resources_dir}}/{{source_file_part}}");
  bundle_data3.SetToolchain(setup.toolchain());
  bundle_data3.visibility().SetPublic();
  ASSERT_TRUE(bundle_data3.OnResolved(&err));

  Target bundle_data4(setup.settings(), Label(SourceDir("//biz/"), "assets"));
  bundle_data4.set_output_type(Target::BUNDLE_DATA);
  bundle_data4.sources().push_back(
      SourceFile("//biz/Biz.xcassets/Contents.json"));
  bundle_data4.sources().push_back(
      SourceFile("//biz/Biz.xcassets/biz.colorset/Contents.json"));
  bundle_data4.action_values().outputs() = SubstitutionList::MakeForTest(
      "{{bundle_resources_dir}}/{{source_file_part}}");
  bundle_data4.SetToolchain(setup.toolchain());
  bundle_data4.visibility().SetPublic();
  ASSERT_TRUE(bundle_data4.OnResolved(&err));

  Target create_bundle(
      setup.settings(),
      Label(SourceDir("//baz/"), "bar", setup.toolchain()->label().dir(),
            setup.toolchain()->label().name()));
  SetupBundleDataDir(&create_bundle.bundle_data(), "//out/Debug");
  create_bundle.set_output_type(Target::CREATE_BUNDLE);
  create_bundle.private_deps().push_back(LabelTargetPair(&bundle_data0));
  create_bundle.private_deps().push_back(LabelTargetPair(&bundle_data1));
  create_bundle.private_deps().push_back(LabelTargetPair(&bundle_data2));
  create_bundle.private_deps().push_back(LabelTargetPair(&bundle_data3));
  create_bundle.private_deps().push_back(LabelTargetPair(&bundle_data4));
  create_bundle.private_deps().push_back(LabelTargetPair(action.get()));
  create_bundle.bundle_data().product_type().assign("com.apple.product-type");
  create_bundle.bundle_data().set_partial_info_plist(
      SourceFile("//out/Debug/baz/bar/bar_partial_info.plist"));
  create_bundle.SetToolchain(setup.toolchain());
  ASSERT_TRUE(create_bundle.OnResolved(&err));

  std::ostringstream out;
  NinjaCreateBundleTargetWriter writer(&create_bundle, out);
  writer.Run();

  const char expected[] =
      "build phony/baz/bar.inputdeps: phony phony/biz/assets "
      "phony/foo/assets phony/foo/bar phony/foo/data "
      "phony/qux/info_plist phony/quz/assets\n"
      "build bar.bundle/Contents/Info.plist: copy_bundle_data "
      "../../qux/qux-Info.plist || phony/baz/bar.inputdeps\n"
      "build bar.bundle/Contents/Resources/input1.txt: copy_bundle_data "
      "../../foo/input1.txt || phony/baz/bar.inputdeps\n"
      "build bar.bundle/Contents/Resources/input2.txt: copy_bundle_data "
      "../../foo/input2.txt || phony/baz/bar.inputdeps\n"
      "build phony/baz/bar.xcassets.inputdeps: phony "
      "phony/foo/assets "
      "phony/quz/assets phony/biz/assets\n"
      "build bar.bundle/Contents/Resources/Assets.car | "
      "baz/bar/bar_partial_info.plist: compile_xcassets "
      "../../foo/Foo.xcassets ../../quz/Quz.xcassets "
      "../../biz/Biz.xcassets | phony/baz/bar.xcassets.inputdeps || "
      "phony/baz/bar.inputdeps\n"
      "  product_type = com.apple.product-type\n"
      "  partial_info_plist = baz/bar/bar_partial_info.plist\n"
      "build phony/baz/bar: phony "
      "bar.bundle/Contents/Info.plist "
      "bar.bundle/Contents/Resources/input1.txt "
      "bar.bundle/Contents/Resources/input2.txt "
      "bar.bundle/Contents/Resources/Assets.car "
      "baz/bar/bar_partial_info.plist || phony/baz/bar.inputdeps\n"
      "build bar.bundle: phony phony/baz/bar\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
}

// Tests post-processing step.
TEST(NinjaCreateBundleTargetWriter, PostProcessing) {
  Err err;
  TestWithScope setup;

  std::unique_ptr<Target> action = NewAction(setup);
  ASSERT_TRUE(action->OnResolved(&err)) << err.message();

  Target executable(setup.settings(), Label(SourceDir("//baz/"), "quz"));
  executable.set_output_type(Target::EXECUTABLE);
  executable.sources().push_back(SourceFile("//baz/quz.c"));
  executable.SetToolchain(setup.toolchain());
  executable.visibility().SetPublic();
  ASSERT_TRUE(executable.OnResolved(&err));

  Target bundle_data(setup.settings(), Label(SourceDir("//foo/"), "data"));
  bundle_data.set_output_type(Target::BUNDLE_DATA);
  bundle_data.sources().push_back(SourceFile("//foo/input1.txt"));
  bundle_data.sources().push_back(SourceFile("//foo/input2.txt"));
  bundle_data.action_values().outputs() = SubstitutionList::MakeForTest(
      "{{bundle_resources_dir}}/{{source_file_part}}");
  bundle_data.SetToolchain(setup.toolchain());
  bundle_data.visibility().SetPublic();
  ASSERT_TRUE(bundle_data.OnResolved(&err));

  Target create_bundle(
      setup.settings(),
      Label(SourceDir("//baz/"), "bar", setup.toolchain()->label().dir(),
            setup.toolchain()->label().name()));
  SetupBundleDataDir(&create_bundle.bundle_data(), "//out/Debug");
  create_bundle.set_output_type(Target::CREATE_BUNDLE);
  create_bundle.bundle_data().set_post_processing_script(
      SourceFile("//build/codesign.py"));
  create_bundle.bundle_data().post_processing_sources().push_back(
      SourceFile("//out/Debug/quz"));
  create_bundle.bundle_data().post_processing_outputs() =
      SubstitutionList::MakeForTest(
          "//out/Debug/bar.bundle/Contents/quz",
          "//out/Debug/bar.bundle/_CodeSignature/CodeResources");
  create_bundle.bundle_data().post_processing_args() =
      SubstitutionList::MakeForTest("-b=quz", "bar.bundle");
  create_bundle.public_deps().push_back(LabelTargetPair(&executable));
  create_bundle.private_deps().push_back(LabelTargetPair(&bundle_data));
  create_bundle.private_deps().push_back(LabelTargetPair(action.get()));
  create_bundle.SetToolchain(setup.toolchain());
  ASSERT_TRUE(create_bundle.OnResolved(&err));

  std::ostringstream out;
  NinjaCreateBundleTargetWriter writer(&create_bundle, out);
  writer.Run();

  const char expected[] =
      "build phony/baz/bar.inputdeps: phony ./quz phony/foo/bar "
      "phony/foo/data\n"
      "rule __baz_bar___toolchain_default__post_processing_rule\n"
      "  command =  ../../build/codesign.py -b=quz bar.bundle\n"
      "  description = POST PROCESSING //baz:bar(//toolchain:default)\n"
      "  restat = 1\n"
      "\n"
      "build bar.bundle/Contents/Resources/input1.txt: copy_bundle_data "
      "../../foo/input1.txt || phony/baz/bar.inputdeps\n"
      "build bar.bundle/Contents/Resources/input2.txt: copy_bundle_data "
      "../../foo/input2.txt || phony/baz/bar.inputdeps\n"
      "build phony/baz/bar.postprocessing.inputdeps: phony "
      "../../build/codesign.py "
      "quz "
      "bar.bundle/Contents/Resources/input1.txt "
      "bar.bundle/Contents/Resources/input2.txt || "
      "phony/baz/bar.inputdeps\n"
      "build bar.bundle/Contents/quz bar.bundle/_CodeSignature/CodeResources: "
      "__baz_bar___toolchain_default__post_processing_rule "
      "| phony/baz/bar.postprocessing.inputdeps\n"
      "build phony/baz/bar: phony "
      "bar.bundle/Contents/quz "
      "bar.bundle/_CodeSignature/CodeResources || phony/baz/bar.inputdeps\n"
      "build bar.bundle: phony phony/baz/bar\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected, out_str) << out_str << "\n" << expected;
}

TEST(NinjaCreateBundleTargetWriter, PostProcessingNoStampFilesCustomToolchain) {
  Err err;
  TestWithScope setup;
  setup.build_settings()->set_no_stamp_files(true);

  Label other_toolchain_label(SourceDir("//other/"), "toolchain");
  setup.settings()->set_toolchain_label(other_toolchain_label);

  std::unique_ptr<Target> action = NewAction(setup);
  ASSERT_TRUE(action->OnResolved(&err)) << err.message();

  Target executable(setup.settings(), Label(SourceDir("//baz/"), "quz"));
  executable.set_output_type(Target::EXECUTABLE);
  executable.sources().push_back(SourceFile("//baz/quz.c"));
  executable.SetToolchain(setup.toolchain());
  executable.visibility().SetPublic();
  ASSERT_TRUE(executable.OnResolved(&err));

  Target bundle_data(setup.settings(), Label(SourceDir("//foo/"), "data"));
  bundle_data.set_output_type(Target::BUNDLE_DATA);
  bundle_data.sources().push_back(SourceFile("//foo/input1.txt"));
  bundle_data.sources().push_back(SourceFile("//foo/input2.txt"));
  bundle_data.action_values().outputs() = SubstitutionList::MakeForTest(
      "{{bundle_resources_dir}}/{{source_file_part}}");
  bundle_data.SetToolchain(setup.toolchain());
  bundle_data.visibility().SetPublic();
  ASSERT_TRUE(bundle_data.OnResolved(&err));

  Target create_bundle(
      setup.settings(),
      Label(SourceDir("//baz/"), "bar", setup.toolchain()->label().dir(),
            setup.toolchain()->label().name()));
  SetupBundleDataDir(&create_bundle.bundle_data(), "//out/Debug");
  create_bundle.set_output_type(Target::CREATE_BUNDLE);
  create_bundle.bundle_data().set_post_processing_script(
      SourceFile("//build/codesign.py"));
  create_bundle.bundle_data().post_processing_sources().push_back(
      SourceFile("//out/Debug/quz"));
  create_bundle.bundle_data().post_processing_outputs() =
      SubstitutionList::MakeForTest(
          "//out/Debug/bar.bundle/Contents/quz",
          "//out/Debug/bar.bundle/_CodeSignature/CodeResources");
  create_bundle.bundle_data().post_processing_args() =
      SubstitutionList::MakeForTest("-b=quz", "bar.bundle");
  create_bundle.public_deps().push_back(LabelTargetPair(&executable));
  create_bundle.private_deps().push_back(LabelTargetPair(&bundle_data));
  create_bundle.private_deps().push_back(LabelTargetPair(action.get()));
  create_bundle.SetToolchain(setup.toolchain());
  ASSERT_TRUE(create_bundle.OnResolved(&err));

  std::ostringstream out;
  NinjaCreateBundleTargetWriter writer(&create_bundle, out);
  writer.Run();

  const char expected[] =
      "build toolchain/phony/baz/bar.inputdeps: phony ./quz "
      "toolchain/phony/foo/bar "
      "toolchain/phony/foo/data\n"
      "rule __baz_bar___toolchain_default__post_processing_rule\n"
      "  command =  ../../build/codesign.py -b=quz bar.bundle\n"
      "  description = POST PROCESSING //baz:bar(//toolchain:default)\n"
      "  restat = 1\n"
      "\n"
      "build bar.bundle/Contents/Resources/input1.txt: "
      "toolchain_copy_bundle_data "
      "../../foo/input1.txt || toolchain/phony/baz/bar.inputdeps\n"
      "build bar.bundle/Contents/Resources/input2.txt: "
      "toolchain_copy_bundle_data "
      "../../foo/input2.txt || toolchain/phony/baz/bar.inputdeps\n"
      "build toolchain/phony/baz/bar.postprocessing.inputdeps: phony "
      "../../build/codesign.py "
      "quz "
      "bar.bundle/Contents/Resources/input1.txt "
      "bar.bundle/Contents/Resources/input2.txt || "
      "toolchain/phony/baz/bar.inputdeps\n"
      "build bar.bundle/Contents/quz bar.bundle/_CodeSignature/CodeResources: "
      "__baz_bar___toolchain_default__post_processing_rule "
      "| toolchain/phony/baz/bar.postprocessing.inputdeps\n"
      "build toolchain/phony/baz/bar: phony "
      "bar.bundle/Contents/quz "
      "bar.bundle/_CodeSignature/CodeResources || "
      "toolchain/phony/baz/bar.inputdeps\n"
      "build bar.bundle: phony toolchain/phony/baz/bar\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
}
