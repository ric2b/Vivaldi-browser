#! /usr/bin/env python
"""
This script adds a license file to a DMG. Requires Xcode and a plain ascii text
license file.
Obviously only runs on a Mac.

Copyright (C) 2011-2013 Jared Hobbs

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
"""
import os
import sys
import tempfile
import optparse


class Path(str):
    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        os.unlink(self)


def mktemp(dir=None, suffix=''):
    (fd, filename) = tempfile.mkstemp(dir=dir, suffix=suffix)
    os.close(fd)
    return Path(filename)

hex_chars = "0123456789ABCDEF"

def code_r_string(str):
  return chr(len(str))+str

def code_r_string_array(str_list):
  return (chr(len(str_list)//256)+chr(len(str_list)%256) +
            "".join([code_r_string(x) for x in str_list]))

def escape_r_string_array(prefix, str_list):
  global hex_chars
  coded_string = code_r_string_array(str_list)
  coded_string_len = len(coded_string)
  escaped_string = ""
  i = 0
  for c in coded_string:
    if i%16 == 0:
      if i >0:
        escaped_string += '"\n'
      escaped_string += prefix + '$"'
    if i %16 != 0 and i%2 == 0:
        escaped_string += " "
    escaped_string += hex_chars[ord(c)//16]+hex_chars[ord(c)%16]
    i += 1
  if i>0:
    escaped_string += '"\n'
  return escaped_string

button_list = [
  "language",
  "Agree",
  "Disagree",
  "Print",
  "Save...",
  "explain",
]

default_translation = {
  "language": "English",
  "Agree": "Agree",
  "Disagree": "Disagree",
  "Print": "Print",
  "Save...": "Save...",
  "explain": """If you agree with the terms press "Agree" to install %(name)s.  If you do not agree , press "Disagree".""",
}


def get_default_translation(options):
  return prepare_translation(default_translation, options)


def prepare_translation(translations, options):
  ret = dict(translations)
  string_dict = dict(name=options.name or "the software")
  ret["explain"] = ret["explain"] % string_dict
  return ret

def escape_r_dict(prefix, translations, fields):
  return escape_r_string_array(prefix, [translations[field] for field in fields])

def escape_r_button_translation(prefix, translations):
  return escape_r_dict(prefix, translations, button_list)

def main(options, args):
    dmgFile, license = args
    with mktemp('.') as tmpFile:
        with open(tmpFile, 'w') as f:
            f.write("""data 'TMPL' (128, "LPic") {
        $"1344 6566 6175 6C74 204C 616E 6775 6167"
        $"6520 4944 4457 5244 0543 6F75 6E74 4F43"
        $"4E54 042A 2A2A 2A4C 5354 430B 7379 7320"
        $"6C61 6E67 2049 4444 5752 441E 6C6F 6361"
        $"6C20 7265 7320 4944 2028 6F66 6673 6574"
        $"2066 726F 6D20 3530 3030 4457 5244 1032"
        $"2D62 7974 6520 6C61 6E67 7561 6765 3F44"
        $"5752 4404 2A2A 2A2A 4C53 5445"
};

data 'LPic' (5000) {
        $"0000 0002 0000 0000 0000 0000 0004 0000"
};

data 'STR#' (5000, "English buttons") {
""" + escape_r_button_translation("        ", get_default_translation(options)) + """
};

data 'STR#' (5002, "English") {
""" + escape_r_button_translation("        ", get_default_translation(options)) + """
};\n\n""")
            with open(license, 'r') as l:
                kind = 'RTF ' if license.lower().endswith('.rtf') else 'TEXT'
                f.write('data \'%s\' (5000, "English") {\n' % kind)
                def escape(s):
                    return s.strip().replace('\\', '\\\\').replace('"', '\\"')

                for line in l:
                    if len(line) < 1000:
                        f.write('    "' + escape(line) + '\\n"\n')
                    else:
                        for liner in line.split('.'):
                            f.write('    "' + escape(liner) + '. \\n"\n')
                f.write('};\n\n')
            f.write("""data 'styl' (5000, "English") {
        $"0003 0000 0000 000C 0009 0014 0000 0000"
        $"0000 0000 0000 0000 0027 000C 0009 0014"
        $"0100 0000 0000 0000 0000 0000 002A 000C"
        $"0009 0014 0000 0000 0000 0000 0000"
};\n""")
        os.system('hdiutil unflatten -quiet "%s"' % dmgFile)
        ret = os.system('%s -a %s -o "%s"' %
                        (options.rez, tmpFile, dmgFile))
        os.system('hdiutil flatten -quiet "%s"' % dmgFile)
        if options.compression is not None:
            os.system('cp %s %s.temp.dmg' % (dmgFile, dmgFile))
            os.remove(dmgFile)
            if options.compression == "bz2":
                os.system('hdiutil convert %s.temp.dmg -format UDBZ -o %s' %
                          (dmgFile, dmgFile))
            elif options.compression == "gz":
                os.system('hdiutil convert %s.temp.dmg -format ' % dmgFile +
                          'UDZO -imagekey zlib-devel=9 -o %s' % dmgFile)
            os.remove('%s.temp.dmg' % dmgFile)
    if ret == 0:
        print "Successfully added license to '%s'" % dmgFile
    else:
        print "Failed to add license to '%s'" % dmgFile

if __name__ == '__main__':
    parser = optparse.OptionParser()
    parser.set_usage("""%prog <dmgFile> <licenseFile> [OPTIONS]
  This program adds a software license agreement to a DMG file.
  It requires Xcode and either a plain ascii text <licenseFile>
  or a <licenseFile.rtf> with the RTF contents.

  See --help for more details.""")
    parser.add_option(
        '--rez',
        '-r',
        action='store',
        default='/Applications/Xcode.app/Contents/Developer/Tools/Rez',
        help='The path to the Rez tool. Defaults to %default'
    )
    parser.add_option(
        '--compression',
        '-c',
        action='store',
        choices=['bz2', 'gz'],
        default=None,
        help='Optionally compress dmg using specified compression type. '
             'Choices are bz2 and gz.'
    )
    parser.add_option(
      '--name',
      help ="Optional name of the software to be installed"
    )
    options, args = parser.parse_args()
    cond = len(args) != 2
    if not os.path.exists(options.rez):
        print 'Failed to find Rez at "%s"!\n' % options.rez
        cond = True
    if cond:
        parser.print_usage()
        sys.exit(1)
    main(options, args)
