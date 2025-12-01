
win_export_args = []
# Define FcPublic appropriately for exports on windows
if host_machine.system() == 'windows'
  win_export_args += '-DFcPublic=__declspec(dllexport)'
  win_export_args += '-DDLL_EXPORT'
endif

# Factor our pattern manipulation code into its own library
# to break the depedency cycle:
# FontConfig pattern handling - Fontations code - FontConfig.
pattern_sources = [ 'fcpat.c', fcstdint_h.to_list() ]
pattern_lib = static_library('patternlib_internal',
                                    pattern_sources,
                                    include_directories: incbase,
                                    dependencies: [deps],
                                    c_args: win_export_args
                    )

fc_sources = files([
  'fcatomic.c',
  'fccache.c',
  'fccfg.c',
  'fccharset.c',
  'fccompat.c',
  'fcdbg.c',
  'fcdefault.c',
  'fcdir.c',
  'fcformat.c',
  'fcfreetype.c',
  'fcfs.c',
  'fcptrlist.c',
  'fchash.c',
  'fcinit.c',
  'fclang.c',
  'fclist.c',
  'fcmatch.c',
  'fcmatrix.c',
  'fcname.c',
  'fcobjs.c',
  'fcrange.c',
  'fcserialize.c',
  'fcstat.c',
  'fcstr.c',
  'fcweight.c',
  'fcxml.c',
  'ftglue.c',
])



fcobjshash_h = cc.preprocess('fcobjshash.gperf.h', include_directories: incbase)
fcobjshash_gperf = custom_target(
  input: fcobjshash_h,
  output: 'fcobjshash.gperf',
  command: ['cutout.py', '@INPUT@', '@OUTPUT@'],
  build_by_default: true,
)

fcobjshash_h = custom_target('fcobjshash.h',
  input: fcobjshash_gperf,
  output: 'fcobjshash.h',
  command: [gperf, '--pic', '-m', '100', '@INPUT@', '--output-file', '@OUTPUT@']
)

lib_fontconfig_sources = [fc_sources, alias_headers, ft_alias_headers, fclang_h, fccase_h, fcobjshash_h, fcstdint_h]
lib_fontconfig_kwargs = {
  'include_directories': incbase,
  'dependencies': [deps, math_dep],
  'link_with': [pattern_lib],
}

