# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

DEPS = [
    'recipe_engine/cipd',
    'recipe_engine/context',
    'recipe_engine/json',
    'recipe_engine/path',
    'recipe_engine/platform',
    'recipe_engine/step',
]

from recipe_engine.recipe_api import Property
from recipe_engine.config import ConfigGroup, Single

PROPERTIES = {
    '$gn/macos_sdk':
        Property(
            help='Properties specifically for the macos_sdk module.',
            param_name='sdk_properties',
            kind=ConfigGroup(  # pylint: disable=line-too-long
                # XCode build version number. Internally maps to an XCode build id like
                # '9c40b'. See
                #
                #   https://chrome-infra-packages.appspot.com/p/infra_internal/ios/xcode/mac/+/
                #
                # For an up to date list of the latest SDK builds.
                sdk_version=Single(str),

                # The CIPD toolchain tool package and version.
                tool_pkg=Single(str),
                tool_ver=Single(str),
            ),
            default={
                'sdk_version':
                    '10b61',
                'tool_package':
                    'infra/tools/mac_toolchain/${platform}',
                'tool_version':
                    'git_revision:434f5462a77e7103f9d610fa5cabc426bb21502e',
            },
        )
}
