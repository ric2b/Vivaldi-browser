# Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

import sys, os, re, types
import argparse
import json
import shutil
import cStringIO
import refresh_file

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

PRINT_NONE = None
PRINT_ALL = "ALL"
WRITE_GRD_FILES = "write grdfiles"
SETUP_RESOURCES = "setup resources"
PRINT_MAIN_SOURCES= "list main sources"
PRINT_SECONDARY_SOURCES = "list secondary sources"
PRINT_MAIN_RESOURCES= "list main resources"
PRINT_SECONDARY_RESOURCES= "list secondary resources"
PRINT_TARGET_SOURCES="list target sources"
PRINT_TARGET_RESOURCES= "list target resources"
PRINT_SPECIAL_UPDATE_SOURCES="list special update sources"
PRINT_SPECIAL_UPDATE_TARGETS="list special update targets"

GOOGLE_SERVICES_REGEX = (
  "(Payments|Cleanup|Drive|"
  "Talk|Cast|Play|Web Store|Cloud|Safe Browsing|OS|"
  "Hangouts|Copresence|Smart Lock|Translate|"
  "Canary|App|Now|Pay)"
)

def REGEXPPATCH(s):
  return s % GOOGLE_SERVICES_REGEX

GOOGLE_REGEX = [
  REGEXPPATCH(r"\bGoogle.Chrome\b(?! (OS|%s))"),
  REGEXPPATCH(r"((?<!OK )|(<!|Ok,? ))\bGoogle\b(?! Chrome\b)(?! (%s))"),
  REGEXPPATCH(r"(?<!\bGoogle )\bChrome\b(?! (OS|%s))"),
  REGEXPPATCH(r"\bChromium\b(?! (OS|%s))"),
]

REPLACE_GOOGLE_EXCEPTIONS =[
  "IDS_NOTIFICATION_WELCOME_TITLE",
  "IDS_HOTWORD_NOTIFICATION_DESCRIPTION",
  "IDS_HOTWORD_NOTIFICATION_BUTTON",
  "IDS_HOTWORD_SEARCH_PREF_CHKBOX",
  "IDS_CONTENT_CONTEXT_SPELLING_ASK_GOOGLE",
  "IDS_CONTENT_CONTEXT_SPELLING_NO_SUGGESTIONS_FROM_GOOGLE",
  "IDS_CONTENT_CONTEXT_SPELLING_BUBBLE_TEXT",
  "IDS_ERRORPAGES_SUGGESTION_GOOGLE_SEARCH_SUMMARY",
  "IDS_PRINT_PREVIEW_CLOUD_PRINT_PROMOTION",
]

sequence_number = 0

MEDIA_EXTENSIONS = [
  "png",
  "html",
  "css",
  "js",
  "mp3",
  "ico",
]

def ScanDir(scandir):
  dirlist = []
  for dirname, _dirs, filenames in os.walk(scandir):
    dirname = os.path.normpath(os.path.relpath(dirname, scandir))
    #assert dirname and not dirname.startswith(".")
    if dirname == ".":
      dirname = None
    dirlist.extend([os.path.join(
                        *filter(None, [dirname, f])).replace("\\", "/"
                      ) for f in filenames
                      if os.path.splitext(f)[1][1:] in MEDIA_EXTENSIONS])
  #for x in dirlist:
  #  print >>sys.stderr, x
  for x in dirlist:
    assert not x.startswith("."), x
    if x.startswith("."):
      raise Exception(x)
  return dirlist

def iter_gritnode(parent, expected, verbose = False):
  for node in parent.ActiveChildren():
    if verbose:
      print >>sys.stderr, node.name, node.attrs
    if isinstance(node, expected):
      yield node
    elif isinstance(node, misc.IfNode):
      if verbose:
        print >>sys.stderr, node.EvaluateCondition(node.attrs['expr'])
      for x in iter_gritnode(node, expected, verbose=verbose):
        yield x

class GritFile(dict):

  def __init__(self, translation_dir="resouces", keep_sequence=False, **params):
    super(GritFile,self).__init__(**params)
    self.resources = None
    self.filename_base = None
    self.filedir = None
    self.translation_names = {}
    self.all_source_files = set()
    self.active_source_files = set()
    self.active_other_files = set()
    self.translation_dir = translation_dir
    self.keep_sequence = keep_sequence

  def _AddFile(self,f):
    for s in [
            None,
            "default_100_percent",
            "default_200_percent",
            "default_300_percent",
          ]:
      pf = os.path.join(*filter(None, [self.filedir,  s, f])).replace("\\", "/")
      rpf = os.path.join(*filter(None, [s, f])).replace("\\", "/")
      if rpf.startswith("./"):
        raise Exception(rpf)
      if os.path.isfile(pf):
        #print >>sys.stderr, "Adding", rpf
        self.active_source_files.add(rpf)

  def __remove_part_nodes(self, node):
    for cur_node in list(node):
      #print >>sys.stderr, "Checking node with type",cur_node.name
      if cur_node.name == "grit-part":
        raise Exception("Grit-part node")
      if cur_node.name != "part":
        continue
      prnt = cur_node.parent
      filename = cur_node.attrs["file"]
      #print >>sys.stderr, "Removing part file", filename, "upgrading to", prnt.name
      for idx in range(0,len(prnt.children)):
        if (prnt.children[idx].name=="part" and
            prnt.children[idx].attrs["file"] == filename):
          del prnt.children[idx]
          #print >>sys.stderr, "Removed part file", filename
          break
      for idx in range(0,len(prnt.mixed_content)):
        if (prnt.mixed_content[idx].name=="part" and
            prnt.mixed_content[idx].attrs["file"] == filename):
          del prnt.mixed_content[idx]
          #print >>sys.stderr, "Removed part file", filename
          break
      for sub_node in list(cur_node.children):
        prnt.AddChild(sub_node)
        self.__remove_part_nodes(sub_node)
        sub_node.parent = prnt
      cur_node.children = []
      cur_node.mixed_content = []

  def Load(self, filename, params = {},
           output_file_name=None, extra_languages=None,
           load_translations = True):
    self.filename_base = os.path.splitext(
           output_file_name or os.path.basename(filename))[0]
    self.orig_filename_base= os.path.splitext(os.path.basename(filename))[0]
    self.filedir = os.path.dirname(filename)
    self.resources = self.__load_resource(filename,
            params,extra_languages=extra_languages,
            load_translations = load_translations)
    #self.all_source_files.add(filename)
    file_list = ScanDir(self.filedir)
    for x in file_list:
      #print >>sys.stderr, "Adding", x, ":", x[0], x.startswith(".")
      if x.startswith("."):
        raise Exception(x)
    self.all_source_files.update(file_list)
    self.active_source_files.update(file_list)

    self["grit"] = dict(self.resources["resources"].attrs)

    if "outputs" in self.resources["nodes"]["internals"]:
      new_top = empty.OutputsNode()
      new_top.name = "outputs"
      _seq, old_top = self.resources["nodes"]["internals"]["outputs"][0]
      new_top.attrs = dict(old_top.attrs)

      for old_node in iter_gritnode(old_top, io.OutputNode):
        #if (self.orig_filename_base != self.filename_base and
        #    old_node.attrs.get("type", None) == "data_package"):
        #  old_node.attrs["filename"] = old_node.attrs["filename"].replace(
        #              self.orig_filename_base+"_", self.filename_base+"_")
        #if self.orig_filename_base == "components_google_chrome_strings":
        #  print >>sys.stderr, old_node.attrs["filename"]

        new_top.AddChild(old_node)
        old_node.parent = new_top
      self["outputs"]= new_top

    if "translations" in self.resources["nodes"]["internals"]:
      new_top = empty.TranslationsNode()
      new_top.name = "translations"
      _seq, old_top = self.resources["nodes"]["internals"]["translations"][0]
      new_top.attrs = dict(old_top.attrs)

      for old_node in iter_gritnode(old_top, io.FileNode):
        new_node = io.FileNode()
        new_node.name = "file"
        new_node.attrs = dict(old_node.attrs)
        if "path" in new_node.attrs:
          self.all_source_files.add(new_node.attrs["path"].replace("\\", "/"))
          new_node.attrs["path"] = os.path.join(os.getcwd(),
                            old_node.ToRealPath(old_node.GetInputPath()))
        new_top.AddChild(new_node)
        new_node.parent = new_top
      self["translations"]= new_top

    if "release" in self.resources["nodes"]["internals"]:
      new_release = misc.ReleaseNode()
      new_release.name = "release"
      _seq, old_top = self.resources["nodes"]["internals"]["release"][0]
      new_release.attrs = dict(old_top.attrs)
      self["release"] = new_release

    if "includes" in self.resources["nodes"]["internals"]:
      _seq, old_top = self.resources["nodes"]["internals"]["includes"][0]
      if self.keep_sequence:
        new_top = old_top
        self.__remove_part_nodes(new_top)
      else:
        new_top = empty.IncludesNode()
        new_top.name = "includes"
        new_top.attrs = dict(old_top.attrs)
      self["includes"] = new_top
      self["include-files"] = {}

      for _seq, old_node in sorted([
                  x for x in self.resources["nodes"].itervalues()
                     if isinstance(x, tuple) and
                        isinstance(x[1], include.IncludeNode)]):
        new_node = include.IncludeNode()
        new_node.name = "include"
        new_node.attrs = dict(old_node.attrs)
        if "file" in new_node.attrs:
          old_file = new_node.attrs["file"].replace("\\", "/")
          new_node.attrs["file"] = old_file
          self._AddFile(old_file)

        self["include-files"][new_node.attrs["name"]] = (_seq, new_node)

    if "messages" in self.resources["nodes"]["internals"]:
      _seq, old_top = self.resources["nodes"]["internals"]["messages"][0]
      if self.keep_sequence:
        new_top = old_top
        self.__remove_part_nodes(new_top)
      else:
        new_top = empty.MessagesNode()
        new_top.name = "messages"
        new_top.attrs = dict(old_top.attrs)
      self["messages"] = new_top
      self["message-entries"] = {}

      for _seq, old_node in sorted([
                  x for x in self.resources["nodes"].itervalues()
                      if  isinstance(x, tuple) and
                          isinstance(x[1], message.MessageNode)]):
        #if old_node.GetCliques():
        #  print old_node.GetCliques()[0].MessageForLanguage("no").GetPresentableContent()
        self["message-entries"][old_node.attrs["name"]] = (_seq, old_node)

    if "structures" in self.resources["nodes"]["internals"]:
      _seq, old_top = self.resources["nodes"]["internals"]["structures"][0]
      if self.keep_sequence:
        new_top = old_top
        self.__remove_part_nodes(new_top)
      else:
        new_top = empty.StructuresNode()
        new_top.name = "structures"
        new_top.attrs = dict(old_top.attrs)
      self["structures"] = new_top
      self["structure-entries"] = {}

      for _seq, old_node in sorted([
                  x for x in self.resources["nodes"].itervalues()
                      if isinstance(x, tuple) and
                         isinstance(x[1], structure.StructureNode)]):
        if self.keep_sequence:
          new_node = old_node
        else:
          new_node = structure.StructureNode()
          new_node.name = "structure"
          new_node.attrs = dict(old_node.attrs)
        if "file" in new_node.attrs:
          old_file = new_node.attrs["file"].replace("\\", "/")
          new_node.attrs["file"] = old_file
          self._AddFile(old_file)

        self["structure-entries"][new_node.attrs["name"]] = (_seq, new_node)

  def __load_resource(self, filename, params = {}, extra_languages=None,
                      load_translations = True):
    global sequence_number
    resources = grd_reader.Parse(filename, **params)

    if extra_languages:
      language_list = []
      for node in iter_gritnode(resources, empty.TranslationsNode):
        for lnode in iter_gritnode(node, io.FileNode):
          language_list.append(lnode.GetLang())
      new_translate = empty.TranslationsNode()
      new_translate.name = "translations"
      for lnode in iter_gritnode(extra_languages, io.FileNode):
        if lnode.GetLang() not in language_list:
          new_translate.AddChild(lnode)
          lnode.parent = new_translate

      resources.AddChild(new_translate)
      new_translate.parent = resources

    resources.SetOutputLanguage('en')
    if load_translations:
      resources.UberClique().keep_additional_translations_ = True
      resources.RunGatherers()

    active_resources = {"internals":{}}
    for node in resources.ActiveDescendants():
      sequence_number += 1
      if "name" in node.attrs:
        if isinstance(node, (message.PhNode, )):
          continue

        active_resources[node.attrs["name"]]= (sequence_number, node)
      else:
        active_resources["internals"].setdefault(node.name,[]).append(
                                                (sequence_number, node))

    assert "identifiers" not in active_resources["internals"],\
           "Support for <identifiers> not implemented"

    return {
          "resources": resources,
          "nodes": active_resources,
        }

  def ReplaceMatches(self, matcher, replace_with, exceptions=[]):
    for name, seq_node in self.get("message-entries", {}).iteritems():
      if name in exceptions:
        continue
      _seq, node = seq_node
      modified_string = False
      for index, part in enumerate(node.mixed_content):
        if not isinstance(part, types.StringTypes):
          continue
        old_string = node.mixed_content[index]
        node.mixed_content[index] = matcher.sub(replace_with, part)
        modified_string = modified_string or node.mixed_content[index] != old_string
        #if matcher.search(part):
        #  print >>sys.stderr, "Replaced", name, "string |",\
        #         part.encode("utf8"), "| with |",\
        #         node.mixed_content[index].encode("utf8") , "|"
      for part in node.children:
        if isinstance(part, (str, unicode)) and matcher.search(part):
          raise Exception("Child of "+name+" still had Google string")

      clique = node.GetCliques()
      if not clique or not modified_string:
        continue
      clique = clique[0]
      translated = clique.AllMessagesThatMatch(re.compile(".*"))
      if isinstance(translated, dict):
        for _lang, tmessage in translated.iteritems():
          parts = tmessage.GetContent()
          for index, part in enumerate(parts):
            if not isinstance(part, (str, unicode)):
              continue
            parts[index] = matcher.sub(replace_with, part)
            #if matcher.search(part):
            #  print >>sys.stderr, "Replaced", name, " translate |",\
            #        part.encode("utf8"), "| with |",\
            #        parts[index].encode("utf8"), "|"
          for part in tmessage.GetContent():
            if isinstance(part, (str, unicode)) and matcher.search(part):
              raise Exception("Translation of "+name+" still had Google string")

  def ReplaceGoogle(self, exceptions=[]):
    for exp in GOOGLE_REGEX:
      matcher = re.compile(exp, re.MULTILINE)
      self.ReplaceMatches(matcher, "Vivaldi", exceptions)

    matcher = re.compile(r"\bchrome(?=://)", re.MULTILINE)

    self.ReplaceMatches(matcher, "vivaldi", exceptions)

    matcher = re.compile(r"http://code.google.com/p/chromium/issues/entry",
                         re.MULTILINE)

    self.ReplaceMatches(matcher, "https://bugs.vivaldi.com/", exceptions)

  def __replace_node(self, new_node, top_node):
    name = new_node.attrs["name"]
    #if name == "IDS_TERMS_HTML":
    #  print >>sys.stderr, "adding IDS_TERMS_HTML"

    for cur_node in top_node:
      if cur_node.attrs.get("name", None) != name:
        continue

      prnt = cur_node.parent or top_node
      _deleted= False
      #if name == "IDS_TERMS_HTML":
      #  print >>sys.stderr, "searching for IDS_TERMS_HTML. Number of items",\
      #        len(prnt.children)
      try:
        for idx in reversed(range(0, len(prnt.children))):
          if prnt.children[idx].attrs.get("name", None) == name:
            del prnt.children[idx]
            _deleted = True
        for idx in reversed(range(0, len(prnt.mixed_content))):
          if prnt.mixed_content[idx].attrs.get("name", None) == name:
            del prnt.mixed_content[idx]
            _deleted = True
      except:
        print >>sys.stderr, "deleting", name, "item", idx, "from", prnt.attrs,\
              "failed"
        raise

      if not _deleted:
        print >>sys.stderr, "unable to remove", name, "from", prnt.attrs
        raise Exception()

    for cur_node in top_node:
      if cur_node.attrs.get("name", None) == name:
        print >>sys.stderr, "found", name
        raise Exception("Failed delete of", name)

    if top_node.GetNodeById(name):
      print >>sys.stderr, "Did not remove", name, "from", top_node.attrs
      raise Exception()

    top_node.AddChild(new_node)
    new_node.parent = top_node

  def Update(self, other, load_translations = True):
    self.active_other_files.update(other.active_source_files)
    if "outputs" in other and "outputs" not in self:
      self["outputs"] = other["outputs"]
    elif "outputs" in other:
      top_node = self["outputs"]
      for node in other["outputs"].ActiveDescendants():
        if not isinstance(node, io.OutputNode):
          continue
        top_node.AddChild(node)
        node.parent = top_node

    if "releases" in other and "releases" not in self:
      self["releases"] = other["releases"]

    if "includes" in other and "includes" not in self:
      self["includes"] = other["includes"]
      self.setdefault("includes", {})

    for name, seq_node in other.get("include-files", {}).iteritems():
      #seq_node[1].attrs["vivaldi-added"]="1"
      if self.keep_sequence:
        self.__replace_node(seq_node[1],
                            self["includes"])
      if name in self.get("include-files", {}):
        self["include-files"][name] = (self["include-files"][name][0],
                                       seq_node[1])
      else:
        self.setdefault("include-files", {})[name] = seq_node

    if "messages" in other and "messages" not in self:
      self["messages"] = other["messages"]
      self.setdefault("message-entries", {})

    for name, seq_node in other.get("message-entries", {}).iteritems():
      #seq_node[1].attrs["vivaldi-added"]="1"
      if self.keep_sequence:
        self.__replace_node(seq_node[1],
                            self["messages"])

      if name in self.get("message-entries", {}):
        if load_translations:
          new_clique = seq_node[1].GetCliques()[0].clique
          old_clique = self["message-entries"][name][1].GetCliques()[0].clique
          for lang in old_clique:
            if lang not in new_clique:
              new_clique[lang] = old_clique[lang]
        self["message-entries"][name] = (self["message-entries"][name][0],
                                         seq_node[1])
      else:
        self.setdefault("message-entries", {})[name] = seq_node

    if "structures" in other and "structures" not in self:
      self["structures"] = other["structures"]
      self.setdefault("structure-entries", {})

    for name, seq_node in other.get("structure-entries", {}).iteritems():
      #seq_node[1].attrs["vivaldi-added"]="1"
      if self.keep_sequence:
        self.__replace_node(seq_node[1],
                            self["structures"])
      if name in self["structure-entries"]:
        self["structure-entries"][name] = (self["structure-entries"][name][0],
                                           seq_node[1])
      else:
        self["structure-entries"][name] = seq_node

    translations = self.Translations() if load_translations  else None
    translations_node = None
    for lang, translation in sorted((translations or {}).iteritems()):
      if not translation or lang == "en" or lang == "x-P-pseudo":
        continue
      if "generated_translations" not in self:
        translations_node = empty.TranslationsNode()
        translations_node.name = "translations"
        old_top = self.get("translations", other.get("translations"))
        translations_node.attrs = dict(old_top.attrs)
        self["generated_translations"] = translations_node

      new_node = io.FileNode()
      new_node.name = "file"
      #print self.translation_dir
      filename = os.path.join(self.translation_dir,
               self.filename_base+"_"+ lang+".xtb").replace("\\", "/")
      self.translation_names[lang] = filename
      new_node.attrs["path"] = filename
      new_node.attrs["lang"] = lang

      translations_node.AddChild(new_node)
      new_node.parent = translations_node

  def GetLanguageFileName(self, lang):
    return self.translation_names[lang];

  def GetAllMainSources(self):
    return list(self.all_source_files)

  def GetActiveSources(self):
    return list(self.active_source_files)

  def GetActiveOtherSources(self):
    return list(self.active_other_files)

  def GetMigrateTargetFiles(self):
    return (list(self.active_source_files | self.active_other_files) +
            list(self.translation_names.itervalues())
            )
  def GetTargetFiles(self):
    return list(self.translation_names.itervalues())

  def RemoveSpecialUpdateSources(self, files, verbose=False):
    if verbose:
      print >>sys.stderr, "Base Dir", self.filedir
    for x in files:
      x = os.path.relpath(x, self.filedir).replace("\\", "/")
      if verbose:
        print >>sys.stderr, "Removing", x
      if x in self.active_source_files:
        self.active_source_files.remove(x)
      else:
        if verbose:
          print >>sys.stderr, "Failed to remove", x
          base = os.path.basename(x)
          matches = [y for y in self.active_source_files
                       if base == os.path.basename(y)]
          for y in matches:
            print >>sys.stderr, "Possible match:", y
        pass

  def Translations(self):
    if "message-entries" not in self:
      return {}

    translations = {}

    for _seq, node in sorted(self["message-entries"].itervalues()):
      clique = node.GetCliques()
      if not clique:
        continue
      clique = clique[0]
      message_id = clique.GetId()
      translated = clique.AllMessagesThatMatch(re.compile(".*"))
      if isinstance(translated, dict):
        for lang, tmessage in translated.iteritems():
          parts = tmessage.GetContent()
          text = "".join([(util.EscapeHtml(x)
                              if isinstance(x, types.StringTypes) else
                              """<ph name="%s" />""" % x.GetPresentation())
                              for x in parts])
          translations.setdefault(lang,{})[message_id] = text

    return translations

  def GetTranslationsCopy(self):
    if "translations" not in self:
      return None

    return self["translations"]

  def __look_for_duplicated_nodes(self, topnode):
    return
    # Debug code
    #vivaldi_names = set()
    #for old_node in topnode:
    #  if "vivaldi-added" in old_node.attrs:
    #    name = old_node.attrs.get("name", None)
    #    if name:
    #      if name in vivaldi_names:
    #        raise Exception(name+ " is duplicated")
    #      vivaldi_names.add(name)
    #
    #for old_node in topnode:
    #  if "vivaldi-added" in old_node.attrs:
    #    continue
    #  if old_node.name == "part":
    #    raise Exception("part node"+ str(old_node.attrs))
    #  name = old_node.attrs.get("name", None)
    #  if name:
    #    if name in vivaldi_names:
    #      raise Exception(name+ " is duplicated")

  def GritNode(self):
    new_resource = misc.GritNode()
    new_resource.name ="grit"
    new_resource.attrs = dict(self["grit"])

    if "outputs" in self:
      new_resource.AddChild(self["outputs"])
      self["outputs"].parent = new_resource

    if "generated_translations" in self:
      new_resource.AddChild(self["generated_translations"])
      self["generated_translations"].parent = new_resource

    if "release" in self:
      new_release = self["release"]
      new_resource.AddChild(new_release)
      new_release.parent = new_resource

      if "includes" in self and self.get("include-files", None):
        new_top = self["includes"]

        if not self.keep_sequence:
          for _seq, old_node in sorted(self["include-files"].itervalues()):
            new_node = include.IncludeNode()
            new_node.name = "include"
            new_node.attrs = dict(old_node.attrs)
            new_top.AddChild(new_node)
            new_node.parent = new_top
        else:
          self.__look_for_duplicated_nodes(new_top)

        new_release.AddChild(new_top)
        new_top.parent = new_release

      if "messages" in self and self.get("message-entries", None):
        new_top =  self["messages"]

        if not self.keep_sequence:
          for _seq, old_node in sorted(self["message-entries"].itervalues()):
            new_top.AddChild(old_node)
            old_node.parent = new_top
        else:
          self.__look_for_duplicated_nodes(new_top)

        new_release.AddChild(new_top)
        new_top.parent = new_release

      if "structures" in self and self.get("structure-entries", None):
        new_top = self["structures"]

        if not self.keep_sequence:
          for _seq, old_node in sorted(self["structure-entries"].itervalues()):
            new_top.AddChild(old_node)
            old_node.parent = new_top
        else:
          self.__look_for_duplicated_nodes(new_top)

        new_release.AddChild(new_top)
        new_top.parent = new_release

    self.__look_for_duplicated_nodes(new_resource)
    return new_resource

def merge_resource(origin_file, overlay_file, target_location, params = {},
                   action=PRINT_NONE, json_file=None, root_dir=None,
                   translation_dir="resources", output_file_name=None,
                   translation_stamp=None):
  util.PathSearcher.Configure([
    os.path.dirname(target_location),
    os.path.dirname(overlay_file),
    os.path.dirname(origin_file),
    ])
  mergeresource = GritFile(translation_dir=translation_dir)
  mergeresource.Load(overlay_file, params, output_file_name=output_file_name,
                     load_translations = (action != SETUP_RESOURCES))

  extra_translations = mergeresource.GetTranslationsCopy()

  mainresource = GritFile(translation_dir=translation_dir, keep_sequence=True)
  mainresource.Load(origin_file, params, output_file_name=output_file_name,
                    extra_languages=extra_translations,
                    load_translations = (action != SETUP_RESOURCES))
  if action in [WRITE_GRD_FILES, PRINT_ALL]:
    mainresource.ReplaceGoogle(exceptions=REPLACE_GOOGLE_EXCEPTIONS)

  mainresource.Update(mergeresource,
                     load_translations = (action != SETUP_RESOURCES))

  new_resource = mainresource.GritNode()

  resource_target_file = mainresource.filename_base+".grd"
  resource_target_dir = os.path.join(target_location, translation_dir)

  try:
    os.makedirs(resource_target_dir)
  except:
    pass # Just ignore the errors here. Most likely the dir exists,
         # otherwise next step will fail instead
  if action in [WRITE_GRD_FILES, PRINT_ALL]:
    for lang, extra_translation_items in mergeresource.resources["resources"].UberClique().additional_translations_.iteritems():
      xtb_callback = mainresource.resources["resources"].UberClique().GenerateXtbParserCallback(lang, override_exisiting=True)
      for tid, string in extra_translation_items.iteritems():
        #print "Item", tid, type(string), string
        xtb_callback(tid, string);
        #translations.setdefault(lang, {})[tid] = util.EscapeHtml(string)
    translations = mainresource.Translations()
    for lang, translation in translations.iteritems():
      if lang == "en" or lang == "x-P-pseudo":
        continue # found in the grd file
      if action == PRINT_ALL:
        print
        print "Writing generated language file",\
               mainresource.GetLanguageFileName(lang)
        print "*"*40
      xtb_filename = os.path.join(target_location,
                                  mainresource.GetLanguageFileName(lang))
      xtbfile = cStringIO.StringIO()
      xtbfile.write("""<?xml version="1.0" encoding="utf-8" ?>\n"""
                    """<!DOCTYPE translationbundle>\n"""
                    """<translationbundle lang="%s">"""
                    % (lang if lang != "nb" else "no"))
      for msgid, text in sorted(translation.iteritems()):
        try:
          xtbfile.write((u"""<translation id="%s">%s</translation>\n""" %
                          (msgid, text)).encode("utf8"))
        except:
          print >>sys.stderr, repr(msgid), "|", repr(text), "|",\
                repr(text.encode("utf8"))
          raise
      xtbfile.write("</translationbundle>\n")
      refresh_file.conditional_refresh_file(xtb_filename, xtbfile.getvalue(),
                                            translation_stamp)
      xtbfile.close()


    if action == PRINT_ALL:
      print
      print "Writing generated file", resource_target_file
      print "*"*40
    content = new_resource.FormatXml("  ", False)
    grd_filename = os.path.join(target_location, resource_target_file)
    grdfile = cStringIO.StringIO()
    grdfile.write("""<?xml version="1.0" encoding="utf-8"?>\n""")
    grdfile.write(content.encode("utf8"))
    refresh_file.conditional_refresh_file(grd_filename, grdfile.getvalue())
    grdfile.close()

    special_update_sources = []
    special_update_targets = []
    special_copy_sources = []
    special_copy_targets = []
  if json_file and root_dir:
    import update_file

    target_name = os.path.basename(target_location)
    update_config = update_file.UpdateConfig(json_file, target_name,
                                             target_location, root_dir)

    special_update_sources= update_config.get_update_sources()
    special_update_targets= update_config.get_update_targets()
    special_copy_sources= update_config.get_copy_sources()
    special_copy_targets= update_config.get_copy_targets()

    update_config.update_files(force=True)
    update_config.copy_files(force=True)

    mainresource.RemoveSpecialUpdateSources(special_update_sources +
                                            special_copy_sources)

  sources = {
    PRINT_MAIN_SOURCES:        mainresource.GetAllMainSources(),
    PRINT_SECONDARY_SOURCES:   mergeresource.GetAllMainSources(),
    PRINT_MAIN_RESOURCES:      mainresource.GetActiveSources(),
    PRINT_SECONDARY_RESOURCES: mainresource.GetActiveOtherSources(),
    PRINT_TARGET_SOURCES:      [
      os.path.normpath(os.path.join(target_location, x)).replace("\\", "/")
          for x in mainresource.GetTargetFiles()],
    PRINT_TARGET_RESOURCES:    [
      os.path.normpath(os.path.join(target_location, x)).replace("\\", "/")
          for x in mainresource.GetMigrateTargetFiles()],
    PRINT_SPECIAL_UPDATE_SOURCES: special_update_sources+special_copy_sources,
    PRINT_SPECIAL_UPDATE_TARGETS: special_update_targets+special_copy_targets,
  }

  if False and action != SETUP_RESOURCES:
    source_dir = os.path.dirname(origin_file)
    # initial copy, to please HTML flattening by grit
    for x in sources[PRINT_MAIN_RESOURCES]:
      try:
        try:
          os.makedirs(os.path.join(target_location, os.path.dirname(x)))
        except:
          pass
        refresh_file.conditional_copy(os.path.join(source_dir,x),
                                      os.path.join(target_location, x))
        #print >>sys.stderr, "Copied", x, "to", target_location
      except shutil.Error:
        continue # ignore, same file
      except:
        print >>sys.stderr, "Copying", x, "failed"
        raise
        pass

    source_dir = os.path.dirname(overlay_file)
    # initial copy, to please HTML flattening by grit
    for x in sources[PRINT_SECONDARY_RESOURCES]:
      try:
        try:
          os.makedirs(os.path.join(target_location, os.path.dirname(x)))
        except:
          pass
        refresh_file.conditional_copy(os.path.join(source_dir,x),
                                      os.path.join(target_location, x))
        #print >>sys.stderr, "Copied", x, "to", target_location
      except shutil.Error:
        continue # ignore, same file
      except:
        print >>sys.stderr, "Copying", x, "failed"
        raise
        pass

  json.dump(sources, open(os.path.join(target_location,
                                      "target_info.json"), "w"))

  return write_targets(action, sources, os.path.dirname(origin_file),
                       os.path.dirname(overlay_file))

def write_targets(action, sources, main_source_location,
                  overlay_source_location):
  explain = {
       PRINT_MAIN_SOURCES:        "Main sources",
       PRINT_SECONDARY_SOURCES:   "other sources",
       PRINT_MAIN_RESOURCES:      "other sources",
       PRINT_SECONDARY_RESOURCES: "Main active sources",
       PRINT_TARGET_SOURCES:      "Active other sources",
       PRINT_TARGET_RESOURCES:    "Migrate targets",
       PRINT_SPECIAL_UPDATE_SOURCES:  "Update sources",
       PRINT_SPECIAL_UPDATE_TARGETS:  "Update targets",
    }

  output = []
  if action == PRINT_ALL:
    actions = list(explain.iterkeys())
  else:
    actions = [action]

  for a in actions:
    if a not in sources:
      continue

    if action == PRINT_ALL:
      output.append("")
      output.append(explain.get(a,a))
      output.append("*"*40)
    for x in sources[a]:
      if action in [PRINT_MAIN_SOURCES, PRINT_MAIN_RESOURCES]:
        x = os.path.join(main_source_location, x)
      elif action in [PRINT_SECONDARY_SOURCES, PRINT_SECONDARY_RESOURCES]:
        x = os.path.join(overlay_source_location, x)
      if (x.replace("\\", "/").find("chromium/chromium") >= 0 or
          x.endswith(".grd")):
        raise Exception(explain.get(a,a)+":"+x)
      output.append(str(x.replace("\\", "/")))
  return output

def perform_list_actions(action, target_location, main_source_location,
                         overlay_source_location):
  sources = json.load(
              open(os.path.join(target_location,"target_info.json"), "r"))
  assert isinstance(sources, dict)
  return write_targets(action, sources, main_source_location,
                       overlay_source_location)

def DoMain(argv):
  argparser = argparse.ArgumentParser(add_help=False);

  action = argparser.add_mutually_exclusive_group(required=True)
  action.add_argument("--build", action="store_const", dest="action",
                      const=WRITE_GRD_FILES)
  action.add_argument("--setup", action="store_const", dest="action",
                      const=SETUP_RESOURCES)
  action.add_argument("--list-main-sources", action="store_const",
                      dest="action", const=PRINT_MAIN_SOURCES)
  action.add_argument("--list-secondary-sources", action="store_const",
                      dest="action", const=PRINT_SECONDARY_SOURCES)
  action.add_argument("--list-main-resources", action="store_const",
                      dest="action", const=PRINT_MAIN_RESOURCES)
  action.add_argument("--list-secondary-resources", action="store_const",
                      dest="action", const=PRINT_SECONDARY_RESOURCES)
  action.add_argument("--list-sources", action="store_const", dest="action",
                      const=PRINT_TARGET_SOURCES)
  action.add_argument("--list-resources", action="store_const", dest="action",
                      const=PRINT_TARGET_RESOURCES)
  action.add_argument("--list-update-sources", action="store_const",
                      dest="action", const=PRINT_SPECIAL_UPDATE_SOURCES)
  action.add_argument("--list-update-targets", action="store_const",
                      dest="action", const=PRINT_SPECIAL_UPDATE_TARGETS)
  action.add_argument("--list-all", action="store_const", dest="action",
                      const=PRINT_ALL)

  argparser.add_argument("-h", action="append", dest="grit_code") # rc-arg, ignored
  argparser.add_argument("-D", action="append", dest="defines")
  argparser.add_argument("-E", action="append", dest="build_env")
  argparser.add_argument("-p", action="append", dest="profile_name")
  argparser.add_argument("-t", default=None, dest="target_platform")
  argparser.add_argument("-f",  default="GRIT_DIR/../gritsettings/resource_ids",
                         dest="idsfile")
  argparser.add_argument("--root", default=None)
  argparser.add_argument("--updatejson", default=None)
  argparser.add_argument("--translation", default=None)
  argparser.add_argument("--translation-stamp", default=None)
  argparser.add_argument("--output-file-name", default=None)

  argparser.add_argument("main_resource")
  argparser.add_argument("secondary_resource")
  argparser.add_argument("target_dir")

  try:
    options = argparser.parse_args(argv)
  except:
    sys.exit(1) # Any errors during parsing cause error

  # Copied from chromium
  # Copyright (c) 2012 The Chromium Authors. All rights reserved.
  # Use of this source code is governed by a BSD-style license that can be
  # found in the LICENSE file.
  defines = {}
  for define in options.defines or []:
    name, val = util.ParseDefine(define)
    defines[name] = val

  for env_pair in options.build_env or []:
    (env_name, env_value) = env_pair.split('=', 1)
    os.environ[env_name] = env_value
  # End copies

  params = dict(
    defines=defines,
    target_platform= options.target_platform,
    first_ids_file=None, #options.idsfile,
  )

  if options.action in [WRITE_GRD_FILES, PRINT_ALL, SETUP_RESOURCES]:
    output = merge_resource(options.main_resource, options.secondary_resource,
                 options.target_dir,
                 action = options.action,
                 params = params,
                 json_file = options.updatejson,
                 root_dir = options.root,
                 translation_dir=options.translation,
                 output_file_name=options.output_file_name,
                 translation_stamp=options.translation_stamp)
  else:
    output = perform_list_actions(options.action, options.target_dir,
                         os.path.dirname(options.main_resource),
                         os.path.dirname(options.secondary_resource))
  return "\n".join(output) if output else ""

if __name__ == '__main__':
    #import cProfile
    #cProfile.run("""output = DoMain(sys.argv[1:])""")
    output = DoMain(sys.argv[1:])
    if output:
      print output
