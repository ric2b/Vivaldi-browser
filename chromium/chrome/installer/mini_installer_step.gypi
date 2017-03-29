{
  # Disable precompiled headers for this project, to avoid
  # linker errors when building with VS 2008.
  'msvs_precompiled_header': '',
  'msvs_precompiled_source': '',

  'variables': {
    # Opt out the common compatibility manifest to work around
    # crbug.com/272660.
    # TODO(yukawa): Enable the common compatibility manifest again.
    'win_exe_compatibility_manifest': '',
  },
  'include_dirs': [
    '../..',
    '<(INTERMEDIATE_DIR)',
    '<(SHARED_INTERMEDIATE_DIR)/chrome',
  ],
  'sources': [
    'mini_installer/chrome.release',
    'mini_installer/mini_installer.rc',
    'mini_installer/mini_installer_resource.h',
    '<(INTERMEDIATE_DIR)/packed_files.rc',
  ],
  'msvs_settings': {
    'VCCLCompilerTool': {
      'EnableIntrinsicFunctions': 'true',
      'BufferSecurityCheck': 'false',
      'BasicRuntimeChecks': '0',
      'ExceptionHandling': '0',
    },
    'VCLinkerTool': {
      'RandomizedBaseAddress': '1',
      'DataExecutionPrevention': '0',
      'AdditionalLibraryDirectories': [
        '<(PRODUCT_DIR)/lib'
      ],
      'DelayLoadDLLs': [],
      'EntryPointSymbol': 'MainEntryPoint',
      'IgnoreAllDefaultLibraries': 'true',
      'OptimizeForWindows98': '1',
      'SubSystem': '2',     # Set /SUBSYSTEM:WINDOWS
      'AdditionalDependencies': [
        'shlwapi.lib',
        'setupapi.lib',
      ],
    },
    'VCManifestTool': {
      'AdditionalManifestFiles': [
        '$(ProjectDir)\\mini_installer\\mini_installer.exe.manifest',
      ],
    },
  },
  'configurations': {
    'Debug_Base': {
      'msvs_settings': {
        'VCCLCompilerTool': {
          'BasicRuntimeChecks': '0',
          'BufferSecurityCheck': 'false',
          'ExceptionHandling': '0',
        },
        'VCLinkerTool': {
          'SubSystem': '2',     # Set /SUBSYSTEM:WINDOWS
          'AdditionalOptions': [
            '/safeseh:no',
            '/dynamicbase:no',
            '/ignore:4199',
            '/ignore:4221',
            '/nxcompat',
          ],
        },
      },
    },
    'Release_Base': {
      'includes': ['../../build/internal/release_defaults.gypi'],
      'msvs_settings': {
        'VCCLCompilerTool': {
          'EnableIntrinsicFunctions': 'true',
          'BasicRuntimeChecks': '0',
          'BufferSecurityCheck': 'false',
          'ExceptionHandling': '0',
          'WholeProgramOptimization': 'false',
        },
        'VCLinkerTool': {
          'SubSystem': '2',     # Set /SUBSYSTEM:WINDOWS
          'Profile': 'false',   # Conflicts with /FIXED
          'AdditionalOptions': [
            '/SAFESEH:NO',
            '/NXCOMPAT',
            '/DYNAMICBASE:NO',
            '/FIXED',
          ],
        },
      },
    },
  },
  'rules': [
    {
      'rule_name': 'installer_archive',
      'extension': 'release',
      'variables': {
        'create_installer_archive_py_path':
          '../tools/build/win/create_installer_archive.py',
      },
      'conditions': [
        ['enable_hidpi == 1', {
          'variables': {
            'enable_hidpi_flag': '--enable_hidpi=1',
          },
        }, {
          'variables': {
            'enable_hidpi_flag': '',
          },
        }],
        ['component == "shared_library"', {
          'variables': {
            #'component_build_flag': '--component_build=1',
            'component_build_flag': '',
          },
        }, {
          'variables': {
            'component_build_flag': '',
          },
          'outputs': [
            '<(PRODUCT_DIR)/<(RULE_INPUT_NAME).packed.7z',
          ],
        }],
        ['disable_nacl==1', {
          'inputs!': [
            '<(PRODUCT_DIR)/nacl64.exe',
            '<(PRODUCT_DIR)/nacl_irt_x86_32.nexe',
            '<(PRODUCT_DIR)/nacl_irt_x86_64.nexe',
          ],
        }],
        ['target_arch=="x64"', {
          'inputs!': [
            '<(PRODUCT_DIR)/nacl64.exe',
            '<(PRODUCT_DIR)/nacl_irt_x86_32.nexe',
          ],
          'variables': {
            'target_arch_flag': '--target_arch=x64',
          },
        }, {
          'variables': {
            'target_arch_flag': '--target_arch=x86',
          },
        }],
        ['icu_use_data_file_flag == 0', {
          'inputs': [
            '<(PRODUCT_DIR)/icudt.dll',
          ],
        }, { # else icu_use_data_file_flag != 0
          'inputs': [
            '<(PRODUCT_DIR)/icudtl.dat',
          ],
        }],
        ['v8_use_external_startup_data == 1', {
          'inputs': [
            '<(PRODUCT_DIR)/natives_blob.bin',
            '<(PRODUCT_DIR)/snapshot_blob.bin',
          ],
        }],
      ],
      'inputs': [
        '<(create_installer_archive_py_path)',
        '<(PRODUCT_DIR)/vivaldi.exe',
        '<(PRODUCT_DIR)/vivaldi.dll',
        '<(PRODUCT_DIR)/nacl64.exe',
        '<(PRODUCT_DIR)/nacl_irt_x86_32.nexe',
        '<(PRODUCT_DIR)/nacl_irt_x86_64.nexe',
        '<(PRODUCT_DIR)/locales/en-US.pak',
      ],
      'outputs': [
        # Also note that chrome.packed.7z is defined as an output in a
        # conditional above.
        'xxx2.out',
        '<(PRODUCT_DIR)/<(RULE_INPUT_NAME).7z',
        '<(PRODUCT_DIR)/setup.ex_',
        '<(INTERMEDIATE_DIR)/packed_files.rc',
      ],
      'action': [
        'python',
        '<(create_installer_archive_py_path)',
        '--build_dir', '<(PRODUCT_DIR)',
        '--staging_dir', '<(INTERMEDIATE_DIR)',
        '--input_file', '<(RULE_INPUT_PATH)',
        '--resource_file_path', '<(INTERMEDIATE_DIR)/packed_files.rc',
        '<(enable_hidpi_flag)',
        '<(component_build_flag)',
        '<(target_arch_flag)',
        '<@(sign_executables)',
        # TODO(sgk):  may just use environment variables
        #'--distribution=$(CHROMIUM_BUILD)',
        '--distribution=vivaldi',
        # Optional arguments to generate diff installer
        #'--last_chrome_installer=C:/Temp/base',
        #'--setup_exe_format=DIFF',
        #'--diff_algorithm=COURGETTE',
        '--vivaldi-version',
        '--vivaldi-build-version', '<(vivaldi_global_build_number)'
      ],
      'message': 'Create installer archive',
    },
  ],
}