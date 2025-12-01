fetch_test_fonts = custom_target(
  'fetch_test_fonts',
  output: 'testfonts',
  command: [
    python3,
    join_paths(meson.project_source_root(), 'build-aux', 'fetch-testfonts.py'),
    '--target-dir',
    '@BUILD_ROOT@/testfonts',
    '--try-symlink',
  ],
  build_by_default: false,
  build_always_stale: true,
)

tests = [
  ['test-bz89617.c', {'c_args': ['-DSRCDIR="@0@"'.format(meson.current_source_dir())]}],
  ['test-bz131804.c'],
  ['test-bz96676.c'],
  ['test-name-parse.c'],
  ['test-bz106618.c'],
  ['test-bz1744377.c'],
  ['test-issue180.c'],
  ['test-family-matching.c'],
  ['test-ptrlist.c', {'include_directories': include_directories('../src'), 'dependencies': libintl_dep}],
]
tests_build_only = [
  ['test-gen-testcache.c', {'include_directories': include_directories('../src'), 'dependencies': libintl_dep}],
]
tests_not_parallel = []

if host_machine.system() != 'windows'
  tests += [
    # FIXME: ['test-migration.c'],
    ['test-bz106632.c', {'c_args': ['-DFONTFILE="@0@"'.format(join_paths(meson.current_source_dir(), '4x6.pcf'))]}],
    ['test-issue107.c'], # FIXME: fails on mingw
  ]
  tests_not_parallel += [
    # FIXME: this needs NotoSans-hinted.zip font downloaded and unpacked into test build directory! see run-test.sh
    ['test-crbug1004254.c', {'dependencies': dependency('threads')}], # for pthread
    ['test-mt-fccfg.c', {'include_directories': include_directories('../src'), 'dependencies': dependency('threads')}],
  ]

  if get_option('default_library') == 'static'
    tests += [
      ['test-issue110.c'],
      ['test-d1f48f11.c'],
    ]
  endif
endif

link_with_libs = [libfontconfig_internal]
if get_option('fontations').enabled()
  link_with_libs += [fc_fontations]
endif

foreach test_data : tests + tests_not_parallel + tests_build_only
  fname = test_data[0]
  opts = test_data.length() > 1 ? test_data[1] : {}
  extra_c_args = opts.get('c_args', [])
  extra_deps = opts.get('dependencies', [])
  extra_incdir = opts.get('include_directories', [])

  test_name = fname.split('.')[0].underscorify()

  exe = executable(test_name, fname, fcstdint_h, fclang_h,
    c_args: c_args + extra_c_args,
    include_directories: [incbase] + extra_incdir,
    link_with: link_with_libs,
    dependencies: extra_deps,
  )

  if test_data not in tests_build_only
    if test_data in tests
      test(test_name, exe, timeout: 600, is_parallel: true)
    else
      test(test_name, exe, timeout: 600, is_parallel: false)
    endif
  endif
endforeach


if get_option('fontations').enabled()
  rust = import('rust')
  rust.test('fc_fontations_rust_tests', fc_fontations, link_with: [libfontconfig_internal], depends: fetch_test_fonts, env: ['builddir=@0@'.format(meson.project_build_root())],)
endif

fs = import('fs')

if conf.get('FREETYPE_PCF_LONG_FAMILY_NAMES')
  out_expected = fs.copyfile('out.expected-long-family-names',
                             'out.expected')
else
  out_expected = fs.copyfile('out.expected-no-long-family-names',
                             'out.expected')
endif

if pytest.found()
  test('pytest', pytest, args: ['--tap'],
       workdir: meson.current_source_dir(),
       env: [
         'builddir=@0@'.format(meson.project_build_root()),
         'srcdir=@0@'.format(meson.project_source_root()),
         'EXEEXT=@0@'.format(conf.get('EXEEXT')),
         'CC=@0@'.format(meson.get_compiler('c').cmd_array()[0]),
       ],
       protocol: 'tap',
       timeout: 600,
       depends: fetch_test_fonts)
endif

if jsonc_dep.found()
  test_conf = executable('test-conf', 'test-conf.c',
                         dependencies: [fontconfig_dep, jsonc_dep])
endif
