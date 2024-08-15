// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/create_bundle_target_generator.h"

#include <map>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "gn/filesystem_utils.h"
#include "gn/label_pattern.h"
#include "gn/parse_tree.h"
#include "gn/scope.h"
#include "gn/substitution_type.h"
#include "gn/target.h"
#include "gn/value.h"
#include "gn/value_extractors.h"
#include "gn/variables.h"

namespace {

// Retrieves value from `scope` named `name` or `old_name`. If the value comes
// from the `old_name` a warning is emitted to inform the name is obsolete.
const Value* GetValueFromScope(Scope* scope,
                               std::string_view name,
                               std::string_view old_name) {
  const Value* value = scope->GetValue(name, true);
  if (value)
    return value;

  value = scope->GetValue(old_name, true);
  if (value) {
    // If there is a value found with the old name, print a warning to the
    // console and use that value. This is to avoid breaking the existing
    // build rules in the wild.
    Err err(*value, "Deprecated variable name",
            base::StringPrintf(
                "The name \"%s\" is deprecated, use \"%s\" instead.",
                std::string(old_name).c_str(), std::string(name).c_str()));
    err.PrintNonfatalToStdout();
  }

  return value;
}

}  // namespace

CreateBundleTargetGenerator::CreateBundleTargetGenerator(
    Target* target,
    Scope* scope,
    const FunctionCallNode* function_call,
    Err* err)
    : TargetGenerator(target, scope, function_call, err) {}

CreateBundleTargetGenerator::~CreateBundleTargetGenerator() = default;

void CreateBundleTargetGenerator::DoRun() {
  target_->set_output_type(Target::CREATE_BUNDLE);

  BundleData& bundle_data = target_->bundle_data();
  if (!FillBundleDir(SourceDir(), variables::kBundleRootDir,
                     &bundle_data.root_dir()))
    return;
  if (!FillBundleDir(bundle_data.root_dir(), variables::kBundleContentsDir,
                     &bundle_data.contents_dir()))
    return;
  if (!FillBundleDir(bundle_data.root_dir(), variables::kBundleResourcesDir,
                     &bundle_data.resources_dir()))
    return;
  if (!FillBundleDir(bundle_data.root_dir(), variables::kBundleExecutableDir,
                     &bundle_data.executable_dir()))
    return;

  if (!FillXcodeExtraAttributes())
    return;

  if (!FillProductType())
    return;

  if (!FillPartialInfoPlist())
    return;

  if (!FillXcodeTestApplicationName())
    return;

  if (!FillPostProcessingScript())
    return;

  if (!FillPostProcessingSources())
    return;

  if (!FillPostProcessingOutputs())
    return;

  if (!FillPostProcessingArgs())
    return;

  if (!FillBundleDepsFilter())
    return;

  if (!FillXcassetCompilerFlags())
    return;

  if (!FillTransparent())
    return;
}

bool CreateBundleTargetGenerator::FillBundleDir(
    const SourceDir& bundle_root_dir,
    std::string_view name,
    SourceDir* bundle_dir) {
  // All bundle_foo_dir properties are optional. They are only required if they
  // are used in an expansion. The check is performed there.
  const Value* value = scope_->GetValue(name, true);
  if (!value)
    return true;
  if (!value->VerifyTypeIs(Value::STRING, err_))
    return false;
  std::string str = value->string_value();
  if (!str.empty() && str[str.size() - 1] != '/')
    str.push_back('/');
  if (!EnsureStringIsInOutputDir(GetBuildSettings()->build_dir(), str,
                                 value->origin(), err_))
    return false;
  if (str != bundle_root_dir.value() &&
      !IsStringInOutputDir(bundle_root_dir, str)) {
    *err_ =
        Err(value->origin(), "Path is not in bundle root dir.",
            base::StringPrintf("The given file should be in the bundle root "
                               "directory or below.Normally you would do "
                               "\"$bundle_root_dir/foo\". I interpreted this "
                               "as \"%s\".",
                               str.c_str()));
    return false;
  }
  *bundle_dir = SourceDir(std::move(str));
  return true;
}

bool CreateBundleTargetGenerator::FillXcodeExtraAttributes() {
  // Need to get a mutable value to mark all values in the scope as used. This
  // cannot be done on a const Scope.
  Value* value = scope_->GetMutableValue(variables::kXcodeExtraAttributes,
                                         Scope::SEARCH_CURRENT, true);
  if (!value)
    return true;

  if (!value->VerifyTypeIs(Value::SCOPE, err_))
    return false;

  Scope* scope_value = value->scope_value();

  Scope::KeyValueMap value_map;
  scope_value->GetCurrentScopeValues(&value_map);
  scope_value->MarkAllUsed();

  std::map<std::string, std::string> xcode_extra_attributes;
  for (const auto& iter : value_map) {
    if (!iter.second.VerifyTypeIs(Value::STRING, err_))
      return false;

    xcode_extra_attributes.insert(
        std::make_pair(std::string(iter.first), iter.second.string_value()));
  }

  target_->bundle_data().xcode_extra_attributes() =
      std::move(xcode_extra_attributes);
  return true;
}

bool CreateBundleTargetGenerator::FillProductType() {
  const Value* value = scope_->GetValue(variables::kProductType, true);
  if (!value)
    return true;

  if (!value->VerifyTypeIs(Value::STRING, err_))
    return false;

  target_->bundle_data().product_type().assign(value->string_value());
  return true;
}

bool CreateBundleTargetGenerator::FillPartialInfoPlist() {
  const Value* value = scope_->GetValue(variables::kPartialInfoPlist, true);
  if (!value)
    return true;

  if (!value->VerifyTypeIs(Value::STRING, err_))
    return false;

  const BuildSettings* build_settings = scope_->settings()->build_settings();
  SourceFile path = scope_->GetSourceDir().ResolveRelativeFile(
      *value, err_, build_settings->root_path_utf8());

  if (err_->has_error())
    return false;

  if (!EnsureStringIsInOutputDir(build_settings->build_dir(), path.value(),
                                 value->origin(), err_))
    return false;

  target_->bundle_data().set_partial_info_plist(path);
  return true;
}

bool CreateBundleTargetGenerator::FillXcodeTestApplicationName() {
  const Value* value =
      scope_->GetValue(variables::kXcodeTestApplicationName, true);
  if (!value)
    return true;

  if (!value->VerifyTypeIs(Value::STRING, err_))
    return false;

  target_->bundle_data().xcode_test_application_name().assign(
      value->string_value());
  return true;
}

bool CreateBundleTargetGenerator::FillPostProcessingScript() {
  const Value* value = GetValueFromScope(
      scope_, variables::kPostProcessingScript, variables::kCodeSigningScript);
  if (!value)
    return true;

  if (!value->VerifyTypeIs(Value::STRING, err_))
    return false;

  SourceFile script_file = scope_->GetSourceDir().ResolveRelativeFile(
      *value, err_, scope_->settings()->build_settings()->root_path_utf8());
  if (err_->has_error())
    return false;

  target_->bundle_data().set_post_processing_script(script_file);
  return true;
}

bool CreateBundleTargetGenerator::FillPostProcessingSources() {
  const Value* value =
      GetValueFromScope(scope_, variables::kPostProcessingSources,
                        variables::kCodeSigningSources);
  if (!value)
    return true;

  if (target_->bundle_data().post_processing_script().is_null()) {
    *err_ = Err(function_call_, "No post-processing script.",
                "You must define post_processing_script if you use "
                "post_processing_sources.");
    return false;
  }

  Target::FileList script_sources;
  if (!ExtractListOfRelativeFiles(scope_->settings()->build_settings(), *value,
                                  scope_->GetSourceDir(), &script_sources,
                                  err_))
    return false;

  target_->bundle_data().post_processing_sources() = std::move(script_sources);
  return true;
}

bool CreateBundleTargetGenerator::FillPostProcessingOutputs() {
  const Value* value =
      GetValueFromScope(scope_, variables::kPostProcessingOutputs,
                        variables::kCodeSigningOutputs);
  if (!value)
    return true;

  if (target_->bundle_data().post_processing_script().is_null()) {
    *err_ = Err(function_call_, "No post-processing script.",
                "You must define post_processing_script if you use "
                "post_processing_outputs.");
    return false;
  }

  if (!value->VerifyTypeIs(Value::LIST, err_))
    return false;

  SubstitutionList& outputs = target_->bundle_data().post_processing_outputs();
  if (!outputs.Parse(*value, err_))
    return false;

  if (outputs.list().empty()) {
    *err_ = Err(function_call_, "Post-processing script has no output.",
                "If you have no outputs, the build system can not tell when "
                "your post-processing script needs to be run.");
    return false;
  }

  // Validate that outputs are in the output dir.
  CHECK_EQ(value->list_value().size(), outputs.list().size());
  for (size_t i = 0; i < value->list_value().size(); ++i) {
    if (!EnsureSubstitutionIsInOutputDir(outputs.list()[i],
                                         value->list_value()[i]))
      return false;
  }

  return true;
}

bool CreateBundleTargetGenerator::FillPostProcessingArgs() {
  const Value* value = GetValueFromScope(scope_, variables::kPostProcessingArgs,
                                         variables::kCodeSigningArgs);
  if (!value)
    return true;

  if (target_->bundle_data().post_processing_script().is_null()) {
    *err_ = Err(function_call_, "No post-processing script.",
                "You must define post_processing_script if you use "
                "post_processing_args.");
    return false;
  }

  if (!value->VerifyTypeIs(Value::LIST, err_))
    return false;

  return target_->bundle_data().post_processing_args().Parse(*value, err_);
}

bool CreateBundleTargetGenerator::FillBundleDepsFilter() {
  const Value* value = scope_->GetValue(variables::kBundleDepsFilter, true);
  if (!value)
    return true;

  if (!value->VerifyTypeIs(Value::LIST, err_))
    return false;

  const SourceDir& current_dir = scope_->GetSourceDir();
  std::vector<LabelPattern>& bundle_deps_filter =
      target_->bundle_data().bundle_deps_filter();
  for (const auto& item : value->list_value()) {
    bundle_deps_filter.push_back(LabelPattern::GetPattern(
        current_dir, scope_->settings()->build_settings()->root_path_utf8(),
        item, err_));
    if (err_->has_error())
      return false;
  }

  return true;
}

bool CreateBundleTargetGenerator::FillXcassetCompilerFlags() {
  const Value* value = scope_->GetValue(variables::kXcassetCompilerFlags, true);
  if (!value)
    return true;

  if (!value->VerifyTypeIs(Value::LIST, err_))
    return false;

  return target_->bundle_data().xcasset_compiler_flags().Parse(*value, err_);
}

bool CreateBundleTargetGenerator::FillTransparent() {
  const Value* value = scope_->GetValue(variables::kTransparent, true);
  if (!value)
    return true;

  if (!value->VerifyTypeIs(Value::BOOLEAN, err_))
    return false;

  target_->bundle_data().set_transparent(value->boolean_value());
  return true;
}
