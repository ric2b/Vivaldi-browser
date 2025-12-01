# Copyright 2025 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Wrapper for providing inputs to a trusted federated program."""

import os
import zipfile

import tensorflow_federated as tff


class ProgramInputProvider:
  """A wrapper class for providing inputs to a trusted federated program.

  Trusted federated programs will expect an instance of this class to be
  provided as an argument to the "trusted_program" function. The program can
  then use the provided ProgramInputProvider instance to access the available
  client data and models.
  """

  def __init__(
      self,
      client_ids: list[str],
      client_data_directory: str,
      model_id_to_zip_file: dict[str, str],
  ):
    """Returns an initialized `ProgramInputProvider`.

    Args:
      client_ids: A list of strings representing the clients from this data
        source. Must not be empty.
      client_data_directory: The directory containing the client data.
      model_id_to_zip_file: A dictionary mapping model ids to the paths of the
        zip files containing the model weights for those models.
    """
    self._client_ids = client_ids
    self._client_data_directory = client_data_directory
    self._model_id_to_zip_file = model_id_to_zip_file

  @property
  def client_ids(self):
    """Returns the list of client ids."""
    return self._client_ids

  @property
  def client_data_directory(self):
    """Returns the directory containing the client data."""
    return self._client_data_directory

  def get_model(self, model_id: str) -> tff.learning.models.FunctionalModel:
    """Returns the `tff.learning.models.FunctionalModel` with the given id."""
    if model_id not in self._model_id_to_zip_file:
      raise ValueError(f'Model id {model_id} not found in model_id_to_zip_file')
    zip_file_path = self._model_id_to_zip_file[model_id]
    model_path = os.path.join(os.path.dirname(zip_file_path), model_id)
    try:
      with zipfile.ZipFile(zip_file_path, 'r') as zip_ref:
        zip_ref.extractall(model_path)
    except zipfile.BadZipFile as e:
      raise ValueError(f'`{zip_file_path}` is not a valid ZIP file.') from e
    except FileNotFoundError as e:
      raise ValueError(f'ZIP file not found at `{zip_file_path}`.') from e
    except Exception as e:
      raise ValueError(f'An unexpected error occurred: {e}') from e
    return tff.learning.models.load_functional_model(model_path)
