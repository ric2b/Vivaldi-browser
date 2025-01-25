# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#
# This file is used to manage the dependencies of the chromium website tooling.
# It is used by gclient to determine what version of each dependency to
# check out, and where.
#
# For more information, please refer to the official documentation:
#   https://sites.google.com/a/chromium.org/dev/developers/how-tos/get-the-code
#
# When adding a new dependency, please update the top-level .gitignore file
# to list the dependency's destination directory.

vars = {
  # condition to allowlist deps to be synced in Cider. Allowlisting is needed
  # because not all deps are compatible with Cider. Once we migrate everything
  # to be compatible we can get rid of this allowlisting mecahnism and remove
  # this condition. Tracking bug for removing this condition: b/349365433
  'non_git_source': 'True',
}

allowed_hosts = [
  'chromium.googlesource.com',
]

use_relative_paths = True

deps = {
  'third_party/node/linux': {
    'dep_type': 'gcs',
    'condition': 'non_git_source',
    'bucket': 'chromium-nodejs',
    'objects': [
      {
        'object_name': '20.11.0/f9a337cfa0e2b92d3e5c671c26b454bd8e99769e',
        'sha256sum': '0ba9cc91698c1f833a1fdc1fe0cb392d825ad484c71b0d84388ac80bfd3d6079',
        'size_bytes': 43716484,
        'generation': 1711567575687220,
      },
    ],
  },
  # The Mac x64/arm64 binaries are downloaded regardless of host architecture
  # since it's possible to cross-compile for the other architecture. This can
  # cause problems for tests that use node if the test device architecture does
  # not match the architecture of the compile machine.
  'third_party/node/mac': {
    'dep_type': 'gcs',
    'condition': 'host_os == "mac" and non_git_source',
    'bucket': 'chromium-nodejs',
    'objects': [
      {
        'object_name': '20.11.0/e3c0fd53caae857309815f3f8de7c2dce49d7bca',
        'sha256sum': '20affacca2480c368b75a1d91ec1a2720604b325207ef0cf39cfef3c235dad19',
        'size_bytes': 40649378,
        'generation': 1711567481181885,
      },
    ],
  },
  'third_party/node/mac_arm64': {
    'dep_type': 'gcs',
    'condition': 'host_os == "mac" and non_git_source',
    'bucket': 'chromium-nodejs',
    'objects': [
      {
        'object_name': '20.11.0/5b5681e12a21cda986410f69e03e6220a21dd4d2',
        'sha256sum': 'cecb99fbb369a9090dddc27e228b66335cd72555b44fa8839ef78e56c51682c5',
        'size_bytes': 38989321,
        'generation': 1711567557161126,
      },
    ],
  },
  'third_party/node/win': {
    'dep_type': 'gcs',
    'condition': 'host_os == "win" and non_git_source',
    'bucket': 'chromium-nodejs',
    'objects': [
      {
        'object_name': '20.11.0/2cb36010af52bc5e2a2d1e3675c10361c80d8f8d',
        'sha256sum': '5da5e201155bb3ea99134b404180adebcfa696b0dbc09571d01a09ca5489f53e',
        'size_bytes': 70017688,
        'generation': 1705443750949255,
      },
    ],
  },
}

hooks = [
  {
    'name': 'fetch_node_modules',
    'pattern': '.',
    'action': [ 'python3',
                'scripts/fetch_node_modules.py'
    ],
  },
  {
    'name': 'fetch_lobs',
    'pattern': '.',
    'action': [
      'vpython3',
      'scripts/fetch_lobs.py',
    ],
  },
]
