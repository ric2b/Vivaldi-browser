fontations = get_option('fontations')

if (fontations.enabled())
  rust = import('rust')

  generated_fontconfig = rust.bindgen(
    input : '../fontconfig/fontconfig.h',
    output : 'fontconfig.rs',
    include_directories : [ '../' ],
    args : [
      '--merge-extern-blocks',
      '--allowlist-item=(FcCharSet.*|FC_(SLANT|WEIGHT|WIDTH)_.*|FcFontSet(Add|Create|Destroy).*|FcLangSet(Create|Destroy|Copy|Add|HasLang)|FcWeightFromOpenType.*)',
      '--raw-line=#![allow(nonstandard_style,unused)]',
      '--raw-line= ',
      '--raw-line=pub mod fcint;',
    ],
    c_args : ['-DBINDGEN_IGNORE_VISIBILITY=1'],
  )

  generated_fcint = rust.bindgen(
    input : '../src/fcint.h',
    output : 'fcint.rs',
    include_directories : [ '../' ],
    args : [
      '--merge-extern-blocks',
      '--allowlist-item=(FcPattern.*|FcRange.*|FC_.*_OBJECT|FcCharSet.*|FcFreeTypeLangSet)',
      '--blocklist-type=(FcCharSet|FcLangSet)',
      '--raw-line=#![allow(nonstandard_style,unused)]',
      '--raw-line= ',
      '--raw-line=pub use crate::FcCharSet; pub use crate::FcLangSet;',
    ],
    c_args : ['-DBINDGEN_IGNORE_VISIBILITY=1'],
  )

  bindgen_lib = static_library(
    'fc_fontations_bindgen',
    sources: [generated_fontconfig, generated_fcint],
    rust_abi : 'rust',
  )

  fontations_query_lib = static_library(
    'fc_fontations_query',
    include_directories : [ '../', '../fontconfig' ],
    sources: ['../src/fcfontations.c', fcstdint_h, alias_headers],
  )

  fc_fontations = static_library(
    'fc_fontations',
    sources: ['mod.rs'],
    link_with: [bindgen_lib, pattern_lib, fontations_query_lib],
    rust_abi: 'c',
    dependencies: [
      dependency('skrifa-0.30-rs'),
      dependency('read-fonts-0.28-rs'),
      dependency('font-types-0.8-rs'),
      dependency('libc-0.2-rs'),
    ],
    install: true,

  )

endif