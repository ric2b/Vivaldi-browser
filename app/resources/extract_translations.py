# Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

import sys, os
import argparse

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__),
                            "..", "..", "chromium", "tools", "grit")))

from grit import grd_reader
#from grit.node import base
from grit.node import empty
from grit.node import include
from grit.node import structure
from grit.node import message
from grit.node import io
from grit.node import misc
from grit import util
from grit import tclib

def getMessage(msg):
  bits = []
  for part in msg.parts:
    if isinstance(part, tclib.Placeholder):
      # Skip this since translators look at msgid to see the pattern for msgstr
      #if isMsgId:
      #  bits.append('<ph name="'+part.presentation+'">'+part.original+'<ex>'+part.example+'</ex></ph>');
      #else:
      bits.append('<ph name="'+part.presentation+'" />');
    else:
      bits.append(part)
  r = ''.join(bits).replace('"', '\\"') # escape quotes
  r = '\\n'.join(r.splitlines())        # escape newlines
  return r.encode("utf8")

generated_translation = set()

def write_message(f, options, node, locale=None):
  translated = ""
  if options.locale:
    try:
      translated = getMessage(node.clique.MessageForLanguage(locale,
                                       node.PseudoIsAllowed(),
                                       node.ShouldFallbackToEnglish(),
                                       missing_translation_is_error = True))
    except:
      return
  name = node.attrs["name"]
  message = getMessage(node.clique.GetMessage())
  clique = node.GetCliques()[0]
  tid = clique.GetId()

  unique = (name, message)
  if unique in generated_translation:
    return
  generated_translation.add(unique)

  print >> f, "#. Description:", node.attrs["desc"].encode("utf8")
  print >> f, "#. TranslationId:", tid
  print >> f, "#. vivaldi-file:", options.vivaldi_file

  print >> f, '#, fuzzy'
  print >> f, 'msgctxt "%s"' % name
  print >> f, 'msgid "%s"' % message
  print >> f, 'msgstr "%s"' % translated
  print >> f

def main():

  argparser = argparse.ArgumentParser();
  argparser.add_argument("--chromium-file")
  argparser.add_argument("--vivaldi-file")
  argparser.add_argument("--locale")
  argparser.add_argument("--messages")
  argparser.add_argument("grd_file")
  argparser.add_argument("output_file")

  options = argparser.parse_args()

  resources = grd_reader.Parse(options.grd_file)
  resources.SetOutputLanguage('en')
  resources.RunGatherers()

  message_list = None
  if options.messages:
    with open(options.messages, "r") as f:
      message_list=[x.strip() for x in f.readlines()]

  with open(options.output_file, "w") as f:
    print >> f, 'msgid ""'
    print >> f, 'msgstr ""'
    if options.locale:
      print >> f, '"Language: %s\\n"' % options.locale
    print >> f, '"MIME-Version: 1.0\\n"'
    print >> f, '"Content-Type: text/plain; charset=UTF-8\\n"'
    print >> f, ''
    for node in resources:
      if (not isinstance(node, message.MessageNode) or
          not node.IsTranslateable() or
          "name" not in node.attrs):
        continue
      if message_list and node.attrs["name"] not in message_list:
        continue

      write_message(f, options, node, options.locale);

if __name__ == "__main__":
  main()
