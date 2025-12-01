# Copyright 2022 Google LLC
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
"""Helper methods for doing runtime type checks."""

from typing import Any, Optional, Tuple, Type, Union

import tensorflow as tf


def _format_name_for_error(name: Optional[Any]) -> str:
  """Formats an optional object name for `check_*` error messages.

  Args:
    name: Optional name of the object being checked. If unspecified, will use a
      placeholder object name instead.

  Returns:
    A formatted name for the object suitable for including in error messages.
  """
  return f'`{name}`' if name else 'argument'


def check_type(
    obj: Any,
    t: Union[Type[Any], Tuple[Type[Any], ...]],
    name: Optional[str] = None,
) -> None:
  """Checks if an object is an instance of a type.

  Args:
    obj: The object to check.
    t: The type to test whether `obj` is an instance or not.
    name: Optional name of the object being checked. Will be included in the
      error message if specified.

  Raises:
    TypeError: If `obj` is not an instance of `t`.
  """
  if not isinstance(obj, t):
    msg_name = _format_name_for_error(name)
    raise TypeError(
        f'Expected {msg_name} to be an instance of type {t!r}, but '
        f'found an instance of type {type(obj)!r}.'
    )


def check_callable(obj: Any, name: Optional[str] = None) -> None:
  """Checks if an object is a Python callable.

  Args:
    obj: The object to check.
    name: Optional name of the object being checked. Will be included in the
      error message if specified.

  Raises:
    TypeError: If `obj` is not a Python callable.
  """
  if not callable(obj):
    msg_name = _format_name_for_error(name)
    raise TypeError(
        f'Expected {msg_name} to be callable, but found an '
        f'instance of {type(obj)!r}.'
    )


def check_dataset(
    obj: Union[
        tf.data.Dataset, tf.compat.v1.data.Dataset, tf.compat.v2.data.Dataset
    ],
    name: Optional[str] = None,
) -> None:
  """Checks that the runtime type of the input is a Tensorflow Dataset.

  Tensorflow has many classes which conform to the Dataset API. This method
  checks each of the known Dataset types.

  Args:
    obj: The input object to check.
    name: Optional name of the object being checked. Will be included in the
      error message if specified.
  """
  dataset_types = (
      tf.data.Dataset,
      tf.compat.v1.data.Dataset,
      tf.compat.v2.data.Dataset,
  )
  if not isinstance(obj, dataset_types):
    msg_name = _format_name_for_error(name)
    raise TypeError(
        f'Expected {msg_name} to be a Dataset; but found an '
        f'instance of {type(obj).__name__}.'
    )
