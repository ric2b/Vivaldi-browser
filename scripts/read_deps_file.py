#!/usr/bin/python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class VarImpl(object):
  """Implement the Var function used within the DEPS file."""

  def __init__(self, local_scope, custom_scope):
    self._local_scope = local_scope
    self._custom_scope = custom_scope

  def Lookup(self, var_name):
    """Implements the Var syntax."""
    if var_name in self._custom_scope:
      return self._custom_scope[var_name]
    elif var_name in self._local_scope.get('vars', {}):
      return self._local_scope['vars'][var_name]
    raise Exception('Var is not defined: %s' % var_name)

def GetDepsContent(deps_path, text=None, return_dict=False, git_url=None):
  """Read a DEPS file and return all the sections."""
  if deps_path:
    deps_file = open(deps_path, 'rU')
    content = deps_file.read()
  else:
    content = text
  custom_scope = {}
  local_scope = {}
  if git_url:
    custom_scope['git_url'] = git_url
    custom_scope['webkit_url'] = git_url+"/chromium/blink.git"
  var = VarImpl(local_scope, custom_scope)
  global_scope = {
      'Var': var.Lookup,
      'deps': {},
      'hooks': [],
  }
  exec(content, global_scope, local_scope)
  local_scope.setdefault('deps', {})
  local_scope.setdefault('hooks', [])
  local_scope.setdefault('vars', {})
  local_scope.setdefault('recursion', None)

  if return_dict:
    return local_scope

  return (local_scope['deps'],
          local_scope['hooks'], local_scope['vars'],
          local_scope['recursion'])
