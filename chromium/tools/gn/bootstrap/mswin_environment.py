
import os
import re
import sys
import subprocess

vs_version = "14.0" # VS2015

def _RegistryQueryBase(sysdir, key, value):
  """Use reg.exe to read a particular key.

  While ideally we might use the win32 module, we would like gyp to be
  python neutral, so for instance cygwin python lacks this module.

  Arguments:
    sysdir: The system subdirectory to attempt to launch reg.exe from.
    key: The registry key to read from.
    value: The particular value to read.
  Return:
    stdout from reg.exe, or None for failure.
  """
  # Skip if not on Windows or Python Win32 setup issue
  if sys.platform not in ('win32', 'cygwin'):
    return None
  # Setup params to pass to and attempt to launch reg.exe
  cmd = [os.path.join(os.environ.get('WINDIR', ''), sysdir, 'reg.exe'),
         'query', key]
  if value:
    cmd.extend(['/v', value])
  p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  # Obtain the stdout from reg.exe, reading to the end so p.returncode is valid
  # Note that the error text may be in [1] in some cases
  text = p.communicate()[0]
  # Check return code from reg.exe; officially 0==success and 1==error
  if p.returncode:
    return None
  return text


def _RegistryQuery(key, value=None):
  r"""Use reg.exe to read a particular key through _RegistryQueryBase.

  First tries to launch from %WinDir%\Sysnative to avoid WoW64 redirection. If
  that fails, it falls back to System32.  Sysnative is available on Vista and
  up and available on Windows Server 2003 and XP through KB patch 942589. Note
  that Sysnative will always fail if using 64-bit python due to it being a
  virtual directory and System32 will work correctly in the first place.

  KB 942589 - http://support.microsoft.com/kb/942589/en-us.

  Arguments:
    key: The registry key.
    value: The particular registry value to read (optional).
  Return:
    stdout from reg.exe, or None for failure.
  """
  text = None
  try:
    text = _RegistryQueryBase('Sysnative', key, value)
  except OSError, e:
    if e.errno == errno.ENOENT:
      text = _RegistryQueryBase('System32', key, value)
    else:
      raise
  return text


def _RegistryGetValueUsingWinReg(key, value):
  """Use the _winreg module to obtain the value of a registry key.

  Args:
    key: The registry key.
    value: The particular registry value to read.
  Return:
    contents of the registry key's value, or None on failure.  Throws
    ImportError if _winreg is unavailable.
  """
  import _winreg
  try:
    root, subkey = key.split('\\', 1)
    assert root == 'HKLM'  # Only need HKLM for now.
    with _winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE, subkey) as hkey:
      return _winreg.QueryValueEx(hkey, value)[0]
  except WindowsError:
    return None


def _RegistryGetValue(key, value):
  """Use _winreg or reg.exe to obtain the value of a registry key.

  Using _winreg is preferable because it solves an issue on some corporate
  environments where access to reg.exe is locked down. However, we still need
  to fallback to reg.exe for the case where the _winreg module is not available
  (for example in cygwin python).

  Args:
    key: The registry key.
    value: The particular registry value to read.
  Return:
    contents of the registry key's value, or None on failure.
  """
  try:
    return _RegistryGetValueUsingWinReg(key, value)
  except ImportError:
    pass

  # Fallback to reg.exe if we fail to import _winreg.
  text = _RegistryQuery(key, value)
  if not text:
    return None
  # Extract value.
  match = re.search(r'REG_\w+\s+([^\r]+)\r\n', text)
  if not match:
    return None
  return match.group(1)

def _ConvertToCygpath(path):
  """Convert to cygwin path if we are using cygwin."""
  if sys.platform == 'cygwin':
    p = subprocess.Popen(['cygpath', path], stdout=subprocess.PIPE)
    path = p.communicate()[0].strip()
  return path

def GetVSPath(version):
  # Old method of searching for which VS version is installed
  # We don't use the 2010-encouraged-way because we also want to get the
  # path to the binaries, which it doesn't offer.
  keys = [r'HKLM\Software\Microsoft\VisualStudio\%s' % version,
          r'HKLM\Software\Wow6432Node\Microsoft\VisualStudio\%s' % version,
          r'HKLM\Software\Microsoft\VCExpress\%s' % version,
          r'HKLM\Software\Wow6432Node\Microsoft\VCExpress\%s' % version]
  for index in range(len(keys)):
    path = _RegistryGetValue(keys[index], 'InstallDir')
    if not path:
      continue
    path = _ConvertToCygpath(path)
    # Check for full.
    full_path = os.path.join(path, 'devenv.exe')
    if os.path.exists(full_path):
      # Add this one.
      return os.path.normpath(os.path.join(path, '..', '..'))
  raise Exception("VS2015 not found")

def _SetupScriptInternal(target_arch):
  """Returns a command (with arguments) to be used to set up the
  environment."""
  # If WindowsSDKDir is set and SetEnv.Cmd exists then we are using the
  # depot_tools build tools and should run SetEnv.Cmd to set up the
  # environment. The check for WindowsSDKDir alone is not sufficient because
  # this is set by running vcvarsall.bat.
  assert target_arch in ('x86', 'x64')
  sdk_dir = os.environ.get('WindowsSDKDir')
  if sdk_dir:
    setup_path = os.path.normpath(os.path.join(sdk_dir, 'Bin/SetEnv.Cmd'))

  shortname = "2015"
  vs_path = GetVSPath(vs_version)
  # We don't use VC/vcvarsall.bat for x86 because vcvarsall calls
  # vcvars32, which it can only find if VS??COMNTOOLS is set, which it
  # isn't always.
  if target_arch == 'x86':
    if (os.environ.get('PROCESSOR_ARCHITECTURE') == 'AMD64' or
        os.environ.get('PROCESSOR_ARCHITEW6432') == 'AMD64'):
      # VS2013 and later, non-Express have a x64-x86 cross that we want
      # to prefer.
      return [os.path.normpath(
         os.path.join(vs_path, 'VC/vcvarsall.bat')), 'amd64_x86']
    # Otherwise, the standard x86 compiler.
    return [os.path.normpath(
      os.path.join(vs_path, 'Common7/Tools/vsvars32.bat'))]
  else:
    assert target_arch == 'x64'
    arg = 'x86_amd64'
    # Use the 64-on-64 compiler if we're not using an express
    # edition and we're running on a 64bit OS.
    if (os.environ.get('PROCESSOR_ARCHITECTURE') == 'AMD64' or
        os.environ.get('PROCESSOR_ARCHITEW6432') == 'AMD64'):
      arg = 'amd64'
    return [os.path.normpath(
        os.path.join(vs_path, 'VC/vcvarsall.bat')), arg]

def SetupScript(target_arch):
  script_data = _SetupScriptInternal(target_arch)
  script_path = script_data[0]
  if not os.path.exists(script_path):
    raise Exception('%s is missing - make sure VC++ tools are installed.' %
                    script_path)
  return script_data

def _ExtractImportantEnvironment(output_of_set):
  """Extracts environment variables required for the toolchain to run from
  a textual dump output by the cmd.exe 'set' command."""
  envvars_to_save = (
      'clcache_.*',
      'goma_.*', # TODO(scottmg): This is ugly, but needed for goma.
      'include',
      'lib',
      'libpath',
      'path',
      'pathext',
      'systemroot',
      'temp',
      'tmp',
      )
  env = {}
  # This occasionally happens and leads to misleading SYSTEMROOT error messages
  # if not caught here.
  if output_of_set.count('=') == 0:
    raise Exception('Invalid output_of_set. Value is:\n%s' % output_of_set)
  for line in output_of_set.splitlines():
    for envvar in envvars_to_save:
      if re.match(envvar + '=', line.lower()):
        var, setting = line.split('=', 1)
        if envvar == 'path':
          # Our own rules (for running gyp-win-tool) and other actions in
          # Chromium rely on python being in the path. Add the path to this
          # python here so that if it's not in the path when ninja is run
          # later, python will still be found.
          setting = os.path.dirname(sys.executable) + os.pathsep + setting
        env[var.upper()] = setting
        break
  for required in ('SYSTEMROOT', 'TEMP', 'TMP'):
    if required not in env:
      raise Exception('Environment variable "%s" '
                      'required to be set to valid path' % required)
  return env

def _FormatAsEnvironmentBlock(envvar_dict):
  """Format as an 'environment block' directly suitable for CreateProcess.
  Briefly this is a list of key=value\0, terminated by an additional \0. See
  CreateProcess documentation for more details."""
  block = ''
  nul = '\0'
  for key, value in envvar_dict.iteritems():
    block += key + '=' + value + nul
  block += nul
  return block

def _ExtractCLPath(output_of_where):
  """Gets the path to cl.exe based on the output of calling the environment
  setup batch file, followed by the equivalent of `where`."""
  # Take the first line, as that's the first found in the PATH.
  for line in output_of_where.strip().splitlines():
    if line.startswith('LOC:'):
      return line[len('LOC:'):].strip()


def GenerateEnvironmentFiles(toplevel_build_dir):
  """It's not sufficient to have the absolute path to the compiler, linker,
  etc. on Windows, as those tools rely on .dlls being in the PATH. We also
  need to support both x86 and x64 compilers within the same build (to support
  msvs_target_platform hackery). Different architectures require a different
  compiler binary, and different supporting environment variables (INCLUDE,
  LIB, LIBPATH). So, we extract the environment here, wrap all invocations
  of compiler tools (cl, link, lib, rc, midl, etc.) via win_tool.py which
  sets up the environment, and then we do not prefix the compiler with
  an absolute path, instead preferring something like "cl.exe" in the rule
  which will then run whichever the environment setup has put in the path.
  When the following procedure to generate environment files does not
  meet your requirement (e.g. for custom toolchains), you can pass
  "-G ninja_use_custom_environment_files" to the gyp to suppress file
  generation and use custom environment files prepared by yourself."""
  archs = ('x86', 'x64')
  cl_paths = {}
  for arch in archs:
    # Extract environment variables for subprocesses.
    args = SetupScript(arch)
    args.extend(('&&', 'set'))
    popen = subprocess.Popen(
        args, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    variables, _ = popen.communicate()
    if popen.returncode != 0:
      raise Exception('"%s" failed with error %d' % (args, popen.returncode))
    env = _ExtractImportantEnvironment(variables)

    env_block = _FormatAsEnvironmentBlock(env)
    print "Writing", os.path.join(toplevel_build_dir, 'environment.' + arch);
    sys.stdout.flush()
    f = open(os.path.join(toplevel_build_dir, 'environment.' + arch), 'wb')
    f.write(env_block)
    f.close()

    # Find cl.exe location for this architecture.
    args = SetupScript(arch)
    args.extend(('&&',
      'for', '%i', 'in', '(cl.exe)', 'do', '@echo', 'LOC:%~$PATH:i'))
    popen = subprocess.Popen(args, shell=True, stdout=subprocess.PIPE)
    output, _ = popen.communicate()
    cl_paths[arch] = _ExtractCLPath(output)
  return cl_paths
