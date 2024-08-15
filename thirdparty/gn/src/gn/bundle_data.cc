// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/bundle_data.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "gn/filesystem_utils.h"
#include "gn/label_pattern.h"
#include "gn/output_file.h"
#include "gn/settings.h"
#include "gn/substitution_writer.h"
#include "gn/target.h"

BundleData::BundleData() = default;

BundleData::~BundleData() = default;

SourceFile BundleData::GetAssetsCatalogDirectory(const SourceFile& source) {
  SourceFile assets_catalog_dir;
  std::string_view path = source.value();
  while (!path.empty()) {
    if (path.ends_with(".xcassets")) {
      assets_catalog_dir = SourceFile(path);
    }

    const auto sep = path.find_last_of("/\\");
    if (sep == std::string_view::npos) {
      break;
    }

    path = path.substr(0, sep);
  }
  return assets_catalog_dir;
}

void BundleData::AddBundleData(const Target* target, bool is_create_bundle) {
  DCHECK_EQ(target->output_type(), Target::BUNDLE_DATA);
  for (const auto& pattern : bundle_deps_filter_) {
    if (pattern.Matches(target->label()))
      return;
  }
  if (transparent_) {
    DCHECK(is_create_bundle);
    if (target->bundle_data().product_type() == product_type_) {
      bundle_deps_.push_back(target);
    } else {
      forwarded_bundle_deps_.push_back(target);
    }
    return;
  }
  if (is_create_bundle) {
    bundle_deps_.push_back(target);
  }
  forwarded_bundle_deps_.push_back(target);
}

void BundleData::OnTargetResolved(Target* owning_target) {
  // Only initialize file_rules_ and assets_catalog_sources for "create_bundle"
  // target (properties are only used by those targets).
  if (owning_target->output_type() != Target::CREATE_BUNDLE)
    return;

  UniqueVector<const Target*> assets_catalog_deps;
  UniqueVector<SourceFile> assets_catalog_sources;

  for (const Target* target : bundle_deps_) {
    SourceFiles file_rule_sources;
    for (const SourceFile& source_file : target->sources()) {
      SourceFile assets_catalog_dir = GetAssetsCatalogDirectory(source_file);
      if (!assets_catalog_dir.is_null()) {
        assets_catalog_sources.push_back(assets_catalog_dir);
        assets_catalog_deps.push_back(target);
      } else {
        file_rule_sources.push_back(source_file);
      }
    }

    if (!file_rule_sources.empty()) {
      DCHECK_EQ(target->action_values().outputs().list().size(), 1u);
      file_rules_.push_back(
          BundleFileRule(target, file_rule_sources,
                         target->action_values().outputs().list()[0]));
    }
  }

  assets_catalog_deps_.insert(assets_catalog_deps_.end(),
                              assets_catalog_deps.begin(),
                              assets_catalog_deps.end());
  assets_catalog_sources_.insert(assets_catalog_sources_.end(),
                                 assets_catalog_sources.begin(),
                                 assets_catalog_sources.end());

  GetSourceFiles(&owning_target->sources());
}

void BundleData::GetSourceFiles(SourceFiles* sources) const {
  for (const BundleFileRule& file_rule : file_rules_) {
    sources->insert(sources->end(), file_rule.sources().begin(),
                    file_rule.sources().end());
  }
  sources->insert(sources->end(), assets_catalog_sources_.begin(),
                  assets_catalog_sources_.end());
  if (!post_processing_script_.is_null()) {
    sources->insert(sources->end(), post_processing_sources_.begin(),
                    post_processing_sources_.end());
  }
}

bool BundleData::GetOutputFiles(const Settings* settings,
                                const Target* target,
                                OutputFiles* outputs,
                                Err* err) const {
  SourceFiles outputs_as_sources;
  if (!GetOutputsAsSourceFiles(settings, target, &outputs_as_sources, err))
    return false;
  for (const SourceFile& source_file : outputs_as_sources)
    outputs->push_back(OutputFile(settings->build_settings(), source_file));
  return true;
}

bool BundleData::GetOutputsAsSourceFiles(const Settings* settings,
                                         const Target* target,
                                         SourceFiles* outputs_as_source,
                                         Err* err) const {
  for (const BundleFileRule& file_rule : file_rules_) {
    for (const SourceFile& source : file_rule.sources()) {
      SourceFile expanded_source_file;
      if (!file_rule.ApplyPatternToSource(settings, target, *this, source,
                                          &expanded_source_file, err))
        return false;
      outputs_as_source->push_back(expanded_source_file);
    }
  }

  if (!assets_catalog_sources_.empty())
    outputs_as_source->push_back(GetCompiledAssetCatalogPath());

  if (!partial_info_plist_.is_null())
    outputs_as_source->push_back(partial_info_plist_);

  if (!post_processing_script_.is_null()) {
    std::vector<SourceFile> post_processing_output_files;
    SubstitutionWriter::GetListAsSourceFiles(post_processing_outputs_,
                                             &post_processing_output_files);
    outputs_as_source->insert(outputs_as_source->end(),
                              post_processing_output_files.begin(),
                              post_processing_output_files.end());
  }

  if (!root_dir_.is_null())
    outputs_as_source->push_back(GetBundleRootDirOutput(settings));

  return true;
}

SourceFile BundleData::GetCompiledAssetCatalogPath() const {
  DCHECK(!assets_catalog_sources_.empty());
  std::string assets_car_path = resources_dir_.value() + "/Assets.car";
  return SourceFile(std::move(assets_car_path));
}

SourceFile BundleData::GetBundleRootDirOutput(const Settings* settings) const {
  std::string root_dir_value = root_dir().value();
  size_t last_separator = root_dir_value.rfind('/');
  if (last_separator != std::string::npos)
    root_dir_value = root_dir_value.substr(0, last_separator);

  return SourceFile(std::move(root_dir_value));
}

SourceDir BundleData::GetBundleRootDirOutputAsDir(
    const Settings* settings) const {
  return SourceDir(GetBundleRootDirOutput(settings).value());
}

SourceDir BundleData::GetBundleDir(const Settings* settings) const {
  return GetBundleRootDirOutput(settings).GetDir();
}
