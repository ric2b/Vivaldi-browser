# Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

import sys, os
import argparse
import polib

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__),
                            "..", "..", "chromium", "tools", "grit")))

from grit import grd_reader
#from grit.node import base
from grit.node import empty
from grit.node import include
from grit.node import structure
from grit.node import message
from grit.node import node_io
from grit.node import misc
from grit import util
from grit import tclib
from xml.sax.saxutils import escape as xml_escape

VIVALDI_FILE = "vivaldi-file"
TRANSLATIONID = "TranslationId"
DESCRIPTION = "Description"

handled_translations = set()

def update_translation(options, resources, translations, entry):
  comments = [x.strip() for x in entry.comment.splitlines()]

  arguments = dict([x.split(":", 1) for x in comments if ":" in x])
  if VIVALDI_FILE not in arguments:
    return

  if arguments[VIVALDI_FILE].strip().split(".")[-1] != options.vivaldi_file:
    return

  if TRANSLATIONID not in arguments:
    return

  translation = entry.msgstr

  tid = arguments[TRANSLATIONID].strip()
  desc = arguments.get(DESCRIPTION, "").strip()

  if tid in handled_translations:
    return
  handled_translations.add(tid)

  if tid in translations:
    if "translation" in translations[tid]:
      translations[tid]["translation"].parts = [translation]
      translations[tid]["translation"].placeholders = []
    else:
      if "node" in translations[tid]:
        desc = translations[tid]["node"].attrs.get("desc", desc)
      item = tclib.Translation(id=tid, text=translation,
                               description = desc)
      translations[tid]["translations"] = item
  else:
    item = tclib.Translation(id=tid, text=translation,
                             description = desc)
    translations.setdefault(tid, {})["translations"] = item

def main():

  argparser = argparse.ArgumentParser();
  argparser.add_argument("--locale")
  argparser.add_argument("--vivaldi-file")
  argparser.add_argument("po_file")
  argparser.add_argument("grd_file")
  argparser.add_argument("xtb_file")

  options = argparser.parse_args()

  defines = dict(
    vivaldi_chromium_file=False,
    use_titlecase=False,
    is_macosx=False,
    is_android=False,
    is_win=True,
    vivaldi_debug=False,
  )

  resources = grd_reader.Parse(options.grd_file, defines=defines)
  resources.SetOutputLanguage('en')
  resources.UberClique().keep_additional_translations_ = True
  resources.RunGatherers()

  locale = options.locale

  translations = {}
  for node in resources:
    if (not isinstance(node, message.MessageNode) or
        not node.IsTranslateable() or
        "name" not in node.attrs):
      continue
    clique = node.GetCliques()[0]
    try:
      string_node = clique.MessageForLanguage(locale,
                                       node.PseudoIsAllowed(),
                                       node.ShouldFallbackToEnglish(),
                                       missing_translation_is_error = True
                                       )
      translations[clique.GetId()] = {"node":node,
                                      "translations":string_node_node}
    except:
      pass

  pofile = polib.pofile(options.po_file)

  for entry in pofile.translated_entries():
    update_translation(options, resources, translations, entry)

  if not translations:
    return

  with open(options.xtb_file, "w", encoding="utf8") as xtbfile:
    print(("""<?xml version="1.0" encoding="utf-8" ?>\n"""
                  """<!DOCTYPE translationbundle>\n"""
                  """<translationbundle lang="%s">""" % locale), file=xtbfile)
    for msgid, nodes in sorted(translations.items()):
      text = xml_escape(nodes["translations"].GetRealContent())

      # unescape <ph/> and \"
      text = text.replace("&lt;ph", "<ph").replace("/&gt;", "/>").replace('\"', '"')
      try:
        print(("""<translation id="%s">%s</translation>""" %
                        (msgid, text)), file=xtbfile)
      except:
        print(repr(msgid), "|", repr(text), "|",\
              repr(text.encode("utf8")), file=sys.stderr)
        raise
    print("</translationbundle>", file=xtbfile)

if __name__ == "__main__":
  main()
