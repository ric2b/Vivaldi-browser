# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Entry point for "link" command."""

import collections
import functools
import itertools
import multiprocessing
import os
import pathlib
import pickle
import posixpath
import re
import string
import sys
import zipfile

from codegen import header_common
from codegen import natives_header
import common
import java_types
import jni_generator
import parse
import proxy


_SWITCH_NUM_TO_BE_INERSERTED_LATER_TOKEN = "<INSERT HERE>"

# All but FULL_CLASS_NAME, which is used only for sorting.
MERGEABLE_KEYS = [
    'CLASS_ACCESSORS',
    'FORWARD_DECLARATIONS',
    'JNI_NATIVE_METHOD',
    'JNI_NATIVE_METHOD_ARRAY',
    'PROXY_NATIVE_SIGNATURES',
    'FORWARDING_PROXY_METHODS',
    'PROXY_NATIVE_METHOD_ARRAY',
    'REGISTER_NATIVES',
]


def _ParseHelper(package_prefix, path):
  return parse.parse_java_file(path, package_prefix=package_prefix)


def _LoadJniObjs(paths, options):
  ret = {}
  if all(p.endswith('.jni.pickle') for p in paths):
    for pickle_path in paths:
      with open(pickle_path, 'rb') as f:
        parsed_files = pickle.load(f)
      ret[pickle_path] = [
          jni_generator.JniObject(pf, options, from_javap=False)
          for pf in parsed_files
      ]
  else:
    func = functools.partial(_ParseHelper, options.package_prefix)
    with multiprocessing.Pool() as pool:
      for pf in pool.imap_unordered(func, paths):
        ret[pf.filename] = [
            jni_generator.JniObject(pf, options, from_javap=False)
        ]

  return ret


def _FilterJniObjs(jni_objs_by_path, options):
  for jni_objs in jni_objs_by_path.values():
    # Remove test-only methods.
    if not options.include_test_only:
      for jni_obj in jni_objs:
        jni_obj.RemoveTestOnlyNatives()
    # Ignoring non-active modules and empty natives lists.
    jni_objs[:] = [
        o for o in jni_objs
        if o.natives and o.module_name == options.module_name
    ]


def _Flatten(jni_objs_by_path, paths):
  return itertools.chain(*(jni_objs_by_path[p] for p in paths))


def _Generate(options, native_sources, java_sources, priority_java_sources):
  """Generates files required to perform JNI registration.

  Generates a srcjar containing a single class, GEN_JNI, that contains all
  native method declarations.

  Optionally generates a header file that provides RegisterNatives to perform
  JNI registration.

  Args:
    options: arguments from the command line
    native_sources: A list of .jni.pickle or .java file paths taken from native
        dependency tree. The source of truth.
    java_sources: A list of .jni.pickle or .java file paths. Used to assert
        against native_sources.
    priority_java_sources: A list of .jni.pickle or .java file paths. Used to
        put these listed java files first in multiplexing.
  """
  native_sources_set = set(native_sources)
  java_sources_set = set(java_sources)

  jni_objs_by_path = _LoadJniObjs(native_sources_set | java_sources_set,
                                  options)
  _FilterJniObjs(jni_objs_by_path, options)

  dicts = []
  for jni_obj in _Flatten(jni_objs_by_path,
                          native_sources_set & java_sources_set):
    dicts.append(DictionaryGenerator(jni_obj, options).Generate())

  priority_java_sources = set(
      priority_java_sources) if priority_java_sources else {}
  # Sort to make output deterministic, and to put priority_java_sources at the
  # top.
  dicts.sort(key=lambda d: (d['FILE_PATH'] not in priority_java_sources, d[
      'FULL_CLASS_NAME']))

  stubs = _GenerateStubsAndAssert(options, jni_objs_by_path, native_sources_set,
                                  java_sources_set)
  combined_dict = {}
  for key in MERGEABLE_KEYS:
    combined_dict[key] = ''.join(d.get(key, '') for d in dicts)

  short_gen_jni_class = proxy.get_gen_jni_class(
      short=True,
      name_prefix=options.module_name,
      package_prefix=options.package_prefix)
  full_gen_jni_class = proxy.get_gen_jni_class(
      short=False,
      name_prefix=options.module_name,
      package_prefix=options.package_prefix)
  if options.use_proxy_hash or options.enable_jni_multiplexing:
    gen_jni_class = short_gen_jni_class
  else:
    gen_jni_class = full_gen_jni_class
  # PROXY_NATIVE_SIGNATURES and PROXY_NATIVE_METHOD_ARRAY will have
  # duplicates for JNI multiplexing since all native methods with similar
  # signatures map to the same proxy. Similarly, there may be multiple switch
  # case entries for the same proxy signatures.
  if options.enable_jni_multiplexing:
    proxy_signatures_list = sorted(
        set(combined_dict['PROXY_NATIVE_SIGNATURES'].split('\n')))
    combined_dict['PROXY_NATIVE_SIGNATURES'] = '\n'.join(
        signature for signature in proxy_signatures_list)

    proxy_native_array_list = sorted(
        set(combined_dict['PROXY_NATIVE_METHOD_ARRAY'].split('},\n')))
    combined_dict['PROXY_NATIVE_METHOD_ARRAY'] = '},\n'.join(
        p for p in proxy_native_array_list if p != '') + '}'
    signature_to_cpp_calls = collections.defaultdict(list)
    for d in dicts:
      for signature, cases in d['SIGNATURE_TO_CPP_CALLS'].items():
        signature_to_cpp_calls[signature].extend(cases)
    combined_dict['FORWARDING_PROXY_METHODS'] = _InsertMultiplexingSwitchCases(
        signature_to_cpp_calls, combined_dict['FORWARDING_PROXY_METHODS'],
        short_gen_jni_class)
    combined_dict['FORWARDING_CALLS'] = _AddForwardingCalls(
        signature_to_cpp_calls, short_gen_jni_class)

  if options.header_path:
    combined_dict['NAMESPACE'] = options.namespace or ''
    header_content = CreateFromDict(gen_jni_class, options, combined_dict)
    with common.atomic_output(options.header_path, mode='w') as f:
      f.write(header_content)

  stub_methods_string = ''.join(stubs)

  with common.atomic_output(options.srcjar_path) as f:
    with zipfile.ZipFile(f, 'w') as srcjar:
      if options.use_proxy_hash or options.enable_jni_multiplexing:
        # J/N.java
        common.add_to_zip_hermetic(
            srcjar,
            f'{short_gen_jni_class.full_name_with_slashes}.java',
            data=CreateProxyJavaFromDict(options, gen_jni_class, combined_dict))
        # org/jni_zero/GEN_JNI.java
        common.add_to_zip_hermetic(
            srcjar,
            f'{full_gen_jni_class.full_name_with_slashes}.java',
            data=CreateProxyJavaFromDict(options,
                                         full_gen_jni_class,
                                         combined_dict,
                                         stub_methods=stub_methods_string,
                                         forwarding=True))
      else:
        # org/jni_zero/GEN_JNI.java
        common.add_to_zip_hermetic(
            srcjar,
            f'{full_gen_jni_class.full_name_with_slashes}.java',
            data=CreateProxyJavaFromDict(options,
                                         gen_jni_class,
                                         combined_dict,
                                         stub_methods=stub_methods_string))


def _GenerateStubsAndAssert(options, jni_objs_by_path, native_sources_set,
                            java_sources_set):
  native_only = native_sources_set - java_sources_set
  java_only = java_sources_set - native_sources_set

  java_only_jni_objs = sorted(_Flatten(jni_objs_by_path, java_only),
                              key=lambda jni_obj: jni_obj.filename)
  native_only_jni_objs = sorted(_Flatten(jni_objs_by_path, native_only),
                                key=lambda jni_obj: jni_obj.filename)
  failed = False
  if not options.add_stubs_for_missing_native and java_only_jni_objs:
    failed = True
    warning_message = '''Failed JNI assertion!
We reference Java files which use JNI, but our native library does not depend on
the corresponding generate_jni().
To bypass this check, add stubs to Java with --add-stubs-for-missing-jni.
Excess Java files:
'''
    sys.stderr.write(warning_message)
    sys.stderr.write('\n'.join(o.filename for o in java_only_jni_objs))
    sys.stderr.write('\n')
  if not options.remove_uncalled_methods and native_only_jni_objs:
    failed = True
    warning_message = '''Failed JNI assertion!
Our native library depends on generate_jnis which reference Java files that we
do not include in our final dex.
To bypass this check, delete these extra methods with --remove-uncalled-jni.
Unneeded Java files:
'''
    sys.stderr.write(warning_message)
    sys.stderr.write('\n'.join(o.filename for o in native_only_jni_objs))
    sys.stderr.write('\n')
  if failed:
    sys.exit(1)

  return [_GenerateStubs(o.proxy_natives) for o in java_only_jni_objs]


def _GenerateStubs(natives):
  final_string = ''
  for native in natives:
    template = string.Template("""

    public static ${RETURN_TYPE} ${METHOD_NAME}(${PARAMS_WITH_TYPES}) {
        throw new RuntimeException("Stub - not implemented!");
    }""")

    final_string += template.substitute({
        'RETURN_TYPE':
        native.proxy_return_type.to_java(),
        'METHOD_NAME':
        native.proxy_name,
        'PARAMS_WITH_TYPES':
        native.proxy_params.to_java_declaration(),
    })
  return final_string


def _InsertMultiplexingSwitchCases(signature_to_cpp_calls,
                                   java_functions_string, short_gen_jni_class):
  switch_case_method_name_re = re.compile('return (\w+)\(')
  java_function_call_re = re.compile('public static \S+ (\w+)\(')
  method_to_switch_num = {}
  for signature, cases in sorted(signature_to_cpp_calls.items()):
    for i, case in enumerate(cases):
      method_name = switch_case_method_name_re.search(case).group(1)
      method_to_switch_num[method_name] = i
      if len(cases) > 1:
        cases[i] = f'''          case {i}:
             {case}'''

  swaps = {}
  for match in java_function_call_re.finditer(java_functions_string):
    java_name = match.group(1)
    cpp_full_name = 'Java_' + common.escape_class_name(java_name)
    switch_num = method_to_switch_num[cpp_full_name]
    replace_location = java_functions_string.find(
        _SWITCH_NUM_TO_BE_INERSERTED_LATER_TOKEN, match.end())
    swaps[replace_location] = switch_num

  # Doing a seperate pass to construct the new string for efficiency - don't
  # want to do thousands of copies of a massive string.
  new_java_functions_string = ""
  prev_loc = 0
  for loc, num in sorted(swaps.items()):
    new_java_functions_string += java_functions_string[prev_loc:loc]
    new_java_functions_string += str(num)
    prev_loc = loc + len(_SWITCH_NUM_TO_BE_INERSERTED_LATER_TOKEN)
  new_java_functions_string += java_functions_string[prev_loc:]
  return new_java_functions_string


def _AddForwardingCalls(signature_to_cpp_calls, short_gen_jni_class):
  fn_def_template = string.Template("""
JNI_BOUNDARY_EXPORT ${RETURN} Java_${PROXY_FN_NAME}(
    JNIEnv* env,
    jclass jcaller,
    ${PARAMS_IN_STUB}) {
${FN_BODY}
}""")
  switch_body_template = string.Template("""
        switch (switch_num) {
${CASES}
          default:
            JNI_ZERO_DCHECK(false);
            return${DEFAULT_RETURN};
        }""")

  forwarding_function_definitons = []
  for signature, cases in sorted(signature_to_cpp_calls.items()):
    param_strings, _ = _GetMultiplexingParamsList(signature.param_types)
    java_class_name = common.escape_class_name(
        short_gen_jni_class.full_name_with_slashes)
    java_function_name = common.escape_class_name(
        _GetMultiplexProxyName(signature))
    proxy_function_name = f'{java_class_name}_{java_function_name}'
    all_cases = '\n'.join(cases)
    if len(cases) > 1:
      fn_body = switch_body_template.substitute({
          'PROXY_FN_NAME':
          proxy_function_name,
          'CASES':
          all_cases,
          'DEFAULT_RETURN':
          '' if signature.return_type.is_void() else ' {}',
      })
    else:
      fn_body = f'        {all_cases}'
    forwarding_function_definitons.append(
        fn_def_template.substitute({
            'RETURN': signature.return_type.to_cpp(),
            'PROXY_FN_NAME': proxy_function_name,
            'PARAMS_IN_STUB': ', '.join(param_strings),
            'FN_BODY': fn_body,
        }))

  return ''.join(s for s in forwarding_function_definitons)


def _SetProxyRegistrationFields(options, gen_jni_class, registration_dict):
  registration_template = string.Template("""\

static const JNINativeMethod kMethods_${ESCAPED_PROXY_CLASS}[] = {
${KMETHODS}
};

namespace {

JNI_ZERO_COMPONENT_BUILD_EXPORT bool ${REGISTRATION_NAME}(JNIEnv* env) {
  const int number_of_methods = std::size(kMethods_${ESCAPED_PROXY_CLASS});

  jni_zero::ScopedJavaLocalRef<jclass> native_clazz =
      jni_zero::GetClass(env, "${PROXY_CLASS}");
  if (env->RegisterNatives(
      native_clazz.obj(),
      kMethods_${ESCAPED_PROXY_CLASS},
      number_of_methods) < 0) {

    jni_zero::internal::HandleRegistrationError(env, native_clazz.obj(), __FILE__);
    return false;
  }

  return true;
}

}  // namespace
""")

  registration_call = string.Template("""\

  // Register natives in a proxy.
  if (!${REGISTRATION_NAME}(env)) {
    return false;
  }
""")

  manual_registration = string.Template("""\
// Method declarations.

${JNI_NATIVE_METHOD_ARRAY}\
${PROXY_NATIVE_METHOD_ARRAY}\

${JNI_NATIVE_METHOD}
// Registration function.

namespace ${NAMESPACE} {

bool RegisterNatives(JNIEnv* env) {\
${REGISTER_PROXY_NATIVES}
${REGISTER_NATIVES}
  return true;
}

}  // namespace ${NAMESPACE}
""")

  short_name = options.use_proxy_hash or options.enable_jni_multiplexing
  sub_dict = {
      'ESCAPED_PROXY_CLASS':
      common.escape_class_name(gen_jni_class.full_name_with_slashes),
      'PROXY_CLASS':
      gen_jni_class.full_name_with_slashes,
      'KMETHODS':
      registration_dict['PROXY_NATIVE_METHOD_ARRAY'],
      'REGISTRATION_NAME':
      _GetRegistrationFunctionName(gen_jni_class.full_name_with_slashes),
  }

  if registration_dict['PROXY_NATIVE_METHOD_ARRAY']:
    proxy_native_array = registration_template.substitute(sub_dict)
    proxy_natives_registration = registration_call.substitute(sub_dict)
  else:
    proxy_native_array = ''
    proxy_natives_registration = ''

  registration_dict['PROXY_NATIVE_METHOD_ARRAY'] = proxy_native_array
  registration_dict['REGISTER_PROXY_NATIVES'] = proxy_natives_registration

  if options.manual_jni_registration:
    registration_dict['MANUAL_REGISTRATION'] = manual_registration.substitute(
        registration_dict)
  else:
    registration_dict['MANUAL_REGISTRATION'] = ''


def CreateProxyJavaFromDict(options,
                            gen_jni_class,
                            registration_dict,
                            stub_methods='',
                            forwarding=False):
  template = string.Template("""\
// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package ${PACKAGE};

// This file is autogenerated by
//     third_party/jni_zero/jni_registration_generator.py
// Please do not change its content.

public class ${CLASS_NAME} {
${FIELDS}
${METHODS}
}
""")

  if forwarding or not (options.use_proxy_hash
                        or options.enable_jni_multiplexing):
    fields = string.Template("""\
    public static final boolean TESTING_ENABLED = ${TESTING_ENABLED};
    public static final boolean REQUIRE_MOCK = ${REQUIRE_MOCK};
""").substitute({
        'TESTING_ENABLED': str(options.enable_proxy_mocks).lower(),
        'REQUIRE_MOCK': str(options.require_mocks).lower(),
    })
  else:
    fields = ''

  if forwarding:
    methods = registration_dict['FORWARDING_PROXY_METHODS']
  else:
    methods = registration_dict['PROXY_NATIVE_SIGNATURES']
  methods += stub_methods

  return template.substitute({
      'CLASS_NAME': gen_jni_class.name,
      'FIELDS': fields,
      'PACKAGE': gen_jni_class.package_with_dots,
      'METHODS': methods
  })


def CreateFromDict(gen_jni_class, options, registration_dict):
  """Returns the content of the header file."""
  header_guard = os.path.splitext(options.header_path)[0].upper() + '_'
  header_guard = re.sub(r'[/.-]', '_', header_guard)

  preamble, epilogue = header_common.header_preamble(
      jni_generator.GetScriptName(),
      gen_jni_class,
      system_includes=['iterator'],  # For std::size().
      user_includes=['third_party/jni_zero/jni_zero_internal.h'],
      header_guard=header_guard)
  registration_dict['PREAMBLE'] = preamble
  registration_dict['EPILOGUE'] = epilogue
  template = string.Template("""\
${PREAMBLE}

${CLASS_ACCESSORS}
// Forward declarations (methods).

${FORWARD_DECLARATIONS}
${FORWARDING_CALLS}
${MANUAL_REGISTRATION}
${EPILOGUE}
""")
  _SetProxyRegistrationFields(options, gen_jni_class, registration_dict)
  if not options.enable_jni_multiplexing:
    registration_dict['FORWARDING_CALLS'] = ''
  if len(registration_dict['FORWARD_DECLARATIONS']) == 0:
    return ''

  return template.substitute(registration_dict)


def _GetMultiplexingParamsList(param_types, java_types=False):
  switch_type = 'int' if java_types else 'jint'
  # Parameters are named after their type, with a unique number per parameter
  # type to make sure the names are unique, even within the same types.
  params_type_count = collections.defaultdict(int)
  params_with_type = [f'{switch_type} switch_num']
  param_names = ['switch_num']
  for t in param_types:
    typename = t.java_type.to_java() if java_types else t.to_cpp()
    params_type_count[typename] += 1
    typename_cleaned = typename.replace('[]', '_array').lower()
    param_name = f'{typename_cleaned}_param{params_type_count[typename]}'
    param_names.append(param_name)
    params_with_type.append(f'{typename} {param_name}')

  return params_with_type, param_names


def _GetRegistrationFunctionName(fully_qualified_class):
  """Returns the register name with a given class."""
  return 'RegisterNative_' + common.escape_class_name(fully_qualified_class)


def _CreateMultiplexedSignature(proxy_signature):
  """Inserts an int parameter as the first parameter."""
  switch_param = java_types.JavaParam(java_types.INT, '_method_idx')
  return java_types.JavaSignature.from_params(
      proxy_signature.return_type,
      java_types.JavaParamList((switch_param, ) + proxy_signature.param_list))


class DictionaryGenerator(object):
  """Generates an inline header file for JNI registration."""
  def __init__(self, jni_obj, options):
    self.options = options
    self.file_path = jni_obj.filename
    self.content_namespace = jni_obj.jni_namespace
    self.natives = jni_obj.natives
    self.proxy_natives = jni_obj.proxy_natives
    self.non_proxy_natives = jni_obj.non_proxy_natives
    self.fully_qualified_class = jni_obj.java_class.full_name_with_slashes
    self.type_resolver = jni_obj.type_resolver
    self.class_name = jni_obj.java_class.name
    self.registration_dict = None
    self.jni_obj = jni_obj
    self.gen_jni_class = proxy.get_gen_jni_class(
        short=options.use_proxy_hash or options.enable_jni_multiplexing,
        name_prefix=options.module_name,
        package_prefix=options.package_prefix)

  def Generate(self):
    # GEN_JNI is handled separately.
    java_classes_with_natives = sorted(
        set(n.java_class for n in self.jni_obj.non_proxy_natives))

    self.registration_dict = {
        'FULL_CLASS_NAME': self.fully_qualified_class,
        'FILE_PATH': self.file_path,
    }
    self.registration_dict['CLASS_ACCESSORS'] = (header_common.class_accessors(
        java_classes_with_natives, self.jni_obj.module_name))

    self._AddForwardDeclaration()
    self._AddJNINativeMethodsArrays(java_classes_with_natives)
    self._AddProxyNativeMethodKStrings()
    self._AddRegisterNativesCalls()
    self._AddRegisterNativesFunctions(java_classes_with_natives)

    self.registration_dict['PROXY_NATIVE_SIGNATURES'] = (''.join(
        _MakeProxySignature(self.options, native)
        for native in self.proxy_natives))

    if self.options.enable_jni_multiplexing:
      self._GetMuxingCalls()

    if self.options.use_proxy_hash or self.options.enable_jni_multiplexing:
      self.registration_dict['FORWARDING_PROXY_METHODS'] = ('\n'.join(
          _MakeForwardingProxy(self.options, self.gen_jni_class, native)
          for native in self.proxy_natives))

    return self.registration_dict

  def _SetDictValue(self, key, value):
    self.registration_dict[key] = jni_generator.WrapOutput(value)

  def _AddForwardDeclaration(self):
    """Add the content of the forward declaration to the dictionary."""
    sb = common.StringBuilder()
    for native in self.natives:
      with sb.statement():
        natives_header.proxy_declaration(sb, self.jni_obj, native)
    self._SetDictValue('FORWARD_DECLARATIONS', sb.to_string())

  def _AddRegisterNativesCalls(self):
    """Add the body of the RegisterNativesImpl method to the dictionary."""

    # Only register if there is at least 1 non-proxy native
    if len(self.non_proxy_natives) == 0:
      return ''

    template = string.Template("""\
  if (!${REGISTER_NAME}(env))
    return false;
""")
    value = {
        'REGISTER_NAME':
        _GetRegistrationFunctionName(self.fully_qualified_class)
    }
    register_body = template.substitute(value)
    self._SetDictValue('REGISTER_NATIVES', register_body)

  def _AddJNINativeMethodsArrays(self, java_classes_with_natives):
    """Returns the implementation of the array of native methods."""
    template = string.Template("""\
static const JNINativeMethod kMethods_${JAVA_CLASS}[] = {
${KMETHODS}
};

""")
    open_namespace = ''
    close_namespace = ''
    if self.content_namespace:
      parts = self.content_namespace.split('::')
      all_namespaces = ['namespace %s {' % ns for ns in parts]
      open_namespace = '\n'.join(all_namespaces) + '\n'
      all_namespaces = ['}  // namespace %s' % ns for ns in parts]
      all_namespaces.reverse()
      close_namespace = '\n'.join(all_namespaces) + '\n\n'

    body = self._SubstituteNativeMethods(java_classes_with_natives, template)
    if body:
      self._SetDictValue('JNI_NATIVE_METHOD_ARRAY', ''.join(
          (open_namespace, body, close_namespace)))

  def _GetKMethodsString(self, clazz):
    if clazz != self.class_name:
      return ''
    ret = [self._GetKMethodArrayEntry(n) for n in self.non_proxy_natives]
    return '\n'.join(ret)

  def _GetKMethodArrayEntry(self, native):
    template = string.Template('    { "${NAME}", "${JNI_DESCRIPTOR}", ' +
                               'reinterpret_cast<void*>(${STUB_NAME}) },')

    name = 'native' + native.cpp_name
    jni_descriptor = native.proxy_signature.to_descriptor()
    stub_name = self.jni_obj.GetStubName(native)

    if native.is_proxy:
      # Literal name of the native method in the class that contains the actual
      # native declaration.
      if self.options.enable_jni_multiplexing:
        class_name = common.escape_class_name(
            self.gen_jni_class.full_name_with_slashes)
        sorted_signature = native.proxy_signature.with_params_reordered()
        name = _GetMultiplexProxyName(sorted_signature)
        stub_name = f'Java_{class_name}_' + common.escape_class_name(name)
        multipliexed_signature = _CreateMultiplexedSignature(sorted_signature)
        jni_descriptor = multipliexed_signature.to_descriptor()
      elif self.options.use_proxy_hash:
        name = native.hashed_proxy_name
      else:
        name = native.proxy_name
    values = {
        'NAME': name,
        'JNI_DESCRIPTOR': jni_descriptor,
        'STUB_NAME': stub_name
    }
    return template.substitute(values)

  def _AddProxyNativeMethodKStrings(self):
    """Returns KMethodString for wrapped native methods in all_classes """

    proxy_k_strings = ('\n'.join(
        self._GetKMethodArrayEntry(p) for p in self.proxy_natives))

    self._SetDictValue('PROXY_NATIVE_METHOD_ARRAY', proxy_k_strings)

  def _SubstituteNativeMethods(self, java_classes, template):
    """Substitutes NAMESPACE, JAVA_CLASS and KMETHODS in the provided
    template."""
    ret = []

    for java_class in java_classes:
      clazz = java_class.name
      full_clazz = java_class.full_name_with_slashes

      kmethods = self._GetKMethodsString(clazz)
      namespace_str = ''
      if self.content_namespace:
        namespace_str = self.content_namespace + '::'
      if kmethods:
        values = {
            'NAMESPACE':
            namespace_str,
            'JAVA_CLASS':
            common.escape_class_name(full_clazz),
            'JAVA_CLASS_ACCESSOR':
            header_common.class_accessor_expression(java_class),
            'KMETHODS':
            kmethods
        }
        ret += [template.substitute(values)]
    return '\n'.join(ret)

  def _AddRegisterNativesFunctions(self, java_classes_with_natives):
    """Returns the code for RegisterNatives."""
    if not java_classes_with_natives:
      return ''
    natives = self._GetRegisterNativesImplString(java_classes_with_natives)
    template = string.Template("""\
JNI_ZERO_COMPONENT_BUILD_EXPORT bool ${REGISTER_NAME}(JNIEnv* env) {
${NATIVES}\
  return true;
}

""")
    values = {
        'REGISTER_NAME':
        _GetRegistrationFunctionName(self.fully_qualified_class),
        'NATIVES': natives
    }
    self._SetDictValue('JNI_NATIVE_METHOD', template.substitute(values))

  def _GetRegisterNativesImplString(self, java_classes_with_natives):
    """Returns the shared implementation for RegisterNatives."""
    template = string.Template("""\
  const int kMethods_${JAVA_CLASS}Size =
      std::size(${NAMESPACE}kMethods_${JAVA_CLASS});
  if (env->RegisterNatives(
      ${JAVA_CLASS_ACCESSOR},
      ${NAMESPACE}kMethods_${JAVA_CLASS},
      kMethods_${JAVA_CLASS}Size) < 0) {
    jni_zero::internal::HandleRegistrationError(env,
        ${JAVA_CLASS_ACCESSOR},
        __FILE__);
    return false;
  }

""")
    return self._SubstituteNativeMethods(java_classes_with_natives, template)

  def _GetMuxingCalls(self):
    # Switch cases are grouped together by the same proxy signatures.
    template = string.Template('return ${STUB_NAME}(env, jcaller${PARAMS});')

    signature_to_cpp_calls = collections.defaultdict(list)
    for native in self.proxy_natives:
      _, param_names = _GetMultiplexingParamsList(native.proxy_param_types)
      param_string = ', '.join(param_names[1:])
      if param_string:
        param_string = ', ' + param_string
      values = {
          # We are forced to call the generated stub instead of the impl because
          # the impl is not guaranteed to have a globally unique name.
          'STUB_NAME': self.jni_obj.GetStubName(native),
          'PARAMS': param_string,
      }
      signature = native.proxy_signature.with_params_reordered()
      signature_to_cpp_calls[signature].append(template.substitute(values))

    self.registration_dict['SIGNATURE_TO_CPP_CALLS'] = signature_to_cpp_calls


_MULTIPLEXED_CHAR_BY_TYPE = {
    '[]': 'A',
    'byte': 'B',
    'char': 'C',
    'double': 'D',
    'float': 'F',
    'int': 'I',
    'long': 'J',
    'Class': 'L',
    'Object': 'O',
    'String': 'R',
    'short': 'S',
    'Throwable': 'T',
    'void': 'V',
    'boolean': 'Z',
}


def _GetShortenedMultiplexingType(type_name):
  # Parameter types could contain multi-dimensional arrays and every
  # instance of [] has to be replaced in the proxy signature name.
  for k, v in _MULTIPLEXED_CHAR_BY_TYPE.items():
    type_name = type_name.replace(k, v)
  return type_name


def _GetMultiplexProxyName(signature):
  # Proxy signatures for methods are named after their return type and
  # parameters to ensure uniqueness, even for the same return types.
  params_list = [
      _GetShortenedMultiplexingType(t.to_java()) for t in signature.param_types
  ]
  params_part = ''
  if params_list:
    params_part = '_' + ''.join(p for p in params_list)

  return_value_part = _GetShortenedMultiplexingType(
      signature.return_type.to_java())
  return '_' + return_value_part + params_part


def _MakeForwardingProxy(options, gen_jni_class, proxy_native):
  template = string.Template("""
    public static ${RETURN_TYPE} ${METHOD_NAME}(${PARAMS_WITH_TYPES}) {
        ${MAYBE_RETURN}${PROXY_CLASS}.${PROXY_METHOD_NAME}(${PARAM_NAMES});
    }""")


  if options.enable_jni_multiplexing:
    sorted_signature = proxy_native.proxy_signature.with_params_reordered()
    param_names = sorted_signature.param_list.to_call_str()
    if not param_names:
      param_names = _SWITCH_NUM_TO_BE_INERSERTED_LATER_TOKEN
    else:
      param_names = _SWITCH_NUM_TO_BE_INERSERTED_LATER_TOKEN + ', ' + param_names
    proxy_method_name = _GetMultiplexProxyName(sorted_signature)
  else:
    proxy_method_name = proxy_native.hashed_proxy_name
    param_names = proxy_native.proxy_params.to_call_str()

  return template.substitute({
      'RETURN_TYPE':
      proxy_native.proxy_return_type.to_java(),
      'METHOD_NAME':
      proxy_native.proxy_name,
      'PARAMS_WITH_TYPES':
      proxy_native.proxy_params.to_java_declaration(),
      'MAYBE_RETURN':
      '' if proxy_native.proxy_return_type.is_void() else 'return ',
      'PROXY_CLASS':
      gen_jni_class.full_name_with_dots,
      'PROXY_METHOD_NAME':
      proxy_method_name,
      'PARAM_NAMES':
      param_names,
  })


def _MakeProxySignature(options, proxy_native):
  params_with_types = proxy_native.proxy_params.to_java_declaration()
  native_method_line = """
    public static native ${RETURN} ${PROXY_NAME}(${PARAMS_WITH_TYPES});"""

  if options.enable_jni_multiplexing:
    # This has to be only one line and without comments because all the proxy
    # signatures will be joined, then split on new lines with duplicates removed
    # since multiple |proxy_native|s map to the same multiplexed signature.
    signature_template = string.Template(native_method_line)

    alt_name = None
    sorted_signature = proxy_native.proxy_signature.with_params_reordered()
    proxy_name = _GetMultiplexProxyName(sorted_signature)
    params_with_types_list, _ = _GetMultiplexingParamsList(
        sorted_signature.param_list, java_types=True)
    params_with_types = ', '.join(params_with_types_list)
  elif options.use_proxy_hash:
    signature_template = string.Template("""
      // Original name: ${ALT_NAME}""" + native_method_line)

    alt_name = proxy_native.proxy_name
    proxy_name = proxy_native.hashed_proxy_name
  else:
    signature_template = string.Template("""
      // Hashed name: ${ALT_NAME}""" + native_method_line)

    # We add the prefix that is sometimes used so that codesearch can find it if
    # someone searches a full method name from the stacktrace.
    alt_name = f'Java_J_N_{proxy_native.hashed_proxy_name}'
    proxy_name = proxy_native.proxy_name

  return_type_str = proxy_native.proxy_return_type.to_java()
  return signature_template.substitute({
      'ALT_NAME': alt_name,
      'RETURN': return_type_str,
      'PROXY_NAME': proxy_name,
      'PARAMS_WITH_TYPES': params_with_types,
  })


def _ParseSourceList(path):
  # Path can have duplicates.
  with open(path) as f:
    return sorted(set(f.read().splitlines()))


def _write_depfile(depfile_path, first_gn_output, inputs):
  def _process_path(path):
    assert not os.path.isabs(path), f'Found abs path in depfile: {path}'
    if os.path.sep != posixpath.sep:
      path = str(pathlib.Path(path).as_posix())
    assert '\\' not in path, f'Found \\ in depfile: {path}'
    return path.replace(' ', '\\ ')

  sb = []
  sb.append(_process_path(first_gn_output))
  if inputs:
    # Sort and uniquify to ensure file is hermetic.
    # One path per line to keep it human readable.
    sb.append(': \\\n ')
    sb.append(' \\\n '.join(sorted(_process_path(p) for p in set(inputs))))
  else:
    sb.append(': ')
  sb.append('\n')

  pathlib.Path(depfile_path).write_text(''.join(sb))


def main(parser, args):
  if not args.enable_proxy_mocks and args.require_mocks:
    parser.error('--require-mocks requires --enable-proxy-mocks.')
  if not args.header_path and args.manual_jni_registration:
    parser.error('--manual-jni-registration requires --header-path.')
  if not args.header_path and args.enable_jni_multiplexing:
    parser.error('--enable-jni-multiplexing requires --header-path.')
  if args.remove_uncalled_methods and not args.native_sources_file:
    parser.error('--remove-uncalled-methods requires --native-sources-file.')
  if args.priority_java_sources_file and not args.enable_jni_multiplexing:
    parser.error('--priority-java-sources is only for multiplexing.')
  if args.enable_jni_multiplexing and args.use_proxy_hash:
    parser.error('--enable-jni-multiplexing cannot work with --use-proxy-hash.')

  java_sources = _ParseSourceList(args.java_sources_file)
  if args.native_sources_file:
    native_sources = _ParseSourceList(args.native_sources_file)
  else:
    if args.add_stubs_for_missing_native:
      # This will create a fully stubbed out GEN_JNI.
      native_sources = []
    else:
      # Just treating it like we have perfect alignment between native and java
      # when only looking at java.
      native_sources = java_sources
  if args.priority_java_sources_file:
    priority_java_sources = _ParseSourceList(args.priority_java_sources_file)
  else:
    priority_java_sources = None

  _Generate(args, native_sources, java_sources, priority_java_sources)

  if args.depfile:
    # GN does not declare a dep on the sources files to avoid circular
    # dependencies, so they need to be listed here.
    all_inputs = native_sources + java_sources + [args.java_sources_file]
    if args.native_sources_file:
      all_inputs.append(args.native_sources_file)
    _write_depfile(args.depfile, args.srcjar_path, all_inputs)
