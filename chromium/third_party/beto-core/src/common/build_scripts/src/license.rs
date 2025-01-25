// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

use cmd_runner::license_checker::LicenseChecker;

pub const LICENSE_CHECKER: LicenseChecker = LicenseChecker {
    ignore: &[
        "**/android/build/**",
        "**/target/**",
        "**/.idea/**",
        "**/cmake-build/**",
        "**/java/build/**",
        "**/java/*/build/**",
        "**/*.toml",
        "**/*.md",
        "**/*.lock",
        "**/*.json",
        "**/*.rsp",
        "**/*.patch",
        "**/*.dockerignore",
        "**/*.apk",
        "**/gradle/*",
        "**/.gradle/*",
        "**/.git*",
        "**/*test*vectors.txt",
        "**/auth_token.txt",
        "**/*.mdb",
        "**/.DS_Store",
        "**/fuzz/corpus/**",
        "**/.*.swp",
        "**/*.vim",
        "**/*.properties",
        "**/third_party/**",
        "**/*.png",
        "**/*.ico",
        "**/node_modules/**",
        "**/.angular/**",
        "**/.editorconfig",
        "**/*.class",
        "**/fuzz/artifacts/**",
        "**/cmake-build-debug/**",
        "**/tags",
    ],
};

#[cfg(test)]
mod tests {
    use super::LICENSE_CHECKER;

    #[test]
    fn new_ignore_is_likely_buggy() {
        LICENSE_CHECKER.check_new_ignore_is_likely_buggy();
    }
}
