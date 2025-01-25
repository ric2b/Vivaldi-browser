# Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

import os
import re

from grit import grd_reader
from grit import util
from grit.node import empty
from grit.node import message
from grit.node import structure
from grit.node import include

GOOGLE_SERVICES_REGEX = (
  "(Payments|Cleanup|Drive|Sheets|Docs|Forms|Slides|"
  "Talk|Cast|Play|Web Store|Cloud|Safe Browsing|OS|"
  "Hangouts|Copresence|Smart Lock|Translate|"
  "Canary|App|Now|Pay|Photos)"
)

def REGEXPPATCH(s):
  return s % GOOGLE_SERVICES_REGEX

GOOGLE_REGEX = [
  REGEXPPATCH(r"\bGoogle.Chrome\b(?! (OS|%s))"),
  REGEXPPATCH(r"\b((?<!OK )(?<!Ok, )(?<!to ))Google(\b|\.?)(?! Chrome\b)(?! (%s))"),
  REGEXPPATCH(r"(?<!\bGoogle )\bChrome\b(?! (OS|%s))"),
  REGEXPPATCH(r"\bChromium\b(?! (OS|%s))"),
  r"\bGoogle\b(?= Password Manager\b)",
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
  "IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS",
  "IDS_SETTINGS_PAYMENTS_MANAGE_CREDIT_CARDS",
  "IDS_PRELOAD_PAGES_STANDARD_PRELOADING_SUMMARY",
  "IDS_PRELOAD_PAGES_STANDARD_PRELOADING_BULLET_FOUR",
  "IDS_PRELOAD_PAGES_EXTENDED_PRELOADING_SUMMARY",
  "IDS_PRELOAD_PAGES_EXTENDED_PRELOADING_BULLET_FOUR",
  "IDS_SETTINGS_ADVANCED_PROTECTION_PROGRAM",
  "IDS_SETTINGS_PRIVACY_GUIDE_SAFE_BROWSING_CARD_ENHANCED_PROTECTION_PRIVACY_DESCRIPTION3",
  "IDS_SETTINGS_SAFEBROWSING_ENHANCED_BULLET_FIVE",
  "IDS_SETTINGS_PASSWORDS_LEAK_DETECTION_SIGNED_OUT_ENABLED_DESC",
  "IDS_SETTINGS_SAFEBROWSING_NONE_DESC",
  "IDS_SAFE_BROWSING_NO_PROTECTION_SUMMARY",
  "IDS_SETTINGS_SIGNIN_ALLOWED",
  "IDS_SETTINGS_SIGNIN_ALLOWED_DESC",
  "IDS_CONTEXTUAL_SEARCH_PROMO_TITLE",
  "IDS_CONTEXTUAL_SEARCH_SEE_BETTER_RESULTS_TITLE",
  "IDS_SETTINGS_PRIVACY_SANDBOX_PAGE_EXPLANATION1_PHASE2",
  "IDS_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_MENU_OPTION"
]

def ReplaceMatches(resources, matcher, replace_with, exceptions=[]):
  for node in resources:
    if "name" not in node.attrs or node.attrs["name"] in exceptions:
      continue
    name = node.attrs["name"]
    old_id = None
    if node.GetCliques():
      old_id = node.GetCliques()[0].GetId()
      assert old_id in resources.UberClique().cliques_ , u"{} not in clique for message {}: {}".format(old_id, name, node.GetCdata())
    if not isinstance(node, message.MessageNode) or isinstance(node, message.PhNode):
      continue
    modified_string = False

    finished = False
    while not finished:
      finished = True
      for index, part in enumerate(node.mixed_content):
        if not isinstance(part, (str,)):
          continue
        if index + 1 >= len(node.mixed_content):
          continue
        if isinstance(node.mixed_content[index+1], str):
          part = part + node.mixed_content[index+1]
          node.mixed_content[index] = part
          del node.mixed_content[index+1]
          finished = False
          break;

    for index, part in enumerate(node.mixed_content):
      if not isinstance(part, (str,)):
        continue
      old_string = part
      node.mixed_content[index] = matcher.sub(replace_with, part)
      modified_string = modified_string or node.mixed_content[index] != old_string
      #if matcher.search(part):
      #  print >>sys.stderr, "Replaced", name, "string |",\
      #         part.encode("utf8"), "| with |",\
      #         node.mixed_content[index].encode("utf8") , "|"
    for part in node.children:
      if isinstance(part, str) and matcher.search(part):
        raise Exception("Child of "+name+" still had Google string")

    clique = node.GetCliques()
    if not clique or not modified_string:
      continue
    node.modified_string = True
    #print("Rewrote {} ({})".format(name, old_id))
    clique = clique[0]
    translated = clique.AllMessagesThatMatch(re.compile(".*"))
    if isinstance(translated, list):
      translated = {"en":translated[0]}
    if isinstance(translated, dict):
      clique.GetMessage().GetContent() # Mark ID of entry dirty, so that it will be regenerated
      assert(clique.GetMessage().dirty)
      for _lang, tmessage in translated.items():
        parts = tmessage.GetContent()

        finished = False
        while not finished:
          finished = True
          for index, part in enumerate(parts):
            if not isinstance(part, (str,)):
              continue
            if index + 1 >= len(parts):
              continue
            if isinstance(parts[index+1], str):
              part = part + parts[index+1]
              parts[index] = part
              del parts[index+1]
              finished = False
              break;
        for index, part in enumerate(parts):
          if not isinstance(part, str):
            continue
          old_part = parts[index]
          parts[index] = matcher.sub(replace_with, part)
          node.modified_string = node.modified_string or old_part != parts[index]
          #if matcher.search(part):
          #  print >>sys.stderr, "Replaced", name, " translate |",\
          #        part.encode("utf8"), "| with |",\
          #        parts[index].encode("utf8"), "|"
        for part in tmessage.GetContent():
          if isinstance(part, str) and matcher.search(part):
            raise Exception("Translation of "+name+" still had Google string")
      new_id = clique.GetId()
      assert new_id != old_id, u"{} did not change for message {}: {}".format(old_id, name, node.GetCdata())
      cliques = resources.UberClique().cliques_
      assert old_id in cliques , u"{} not in clique for message {}: {}".format(old_id, name, node.GetCdata())
      old_clique_list = cliques[old_id]
      assert clique in old_clique_list
      cliques.setdefault(new_id, []).append(clique)
      old_clique_list.pop(old_clique_list.index(clique))

def ReplaceGoogle(resources, exceptions=[]):
  for exp in GOOGLE_REGEX:
    matcher = re.compile(exp, re.MULTILINE)
    ReplaceMatches(resources, matcher, "Vivaldi", exceptions)

  matcher = re.compile(r"\bchrome(?=://)", re.MULTILINE)

  ReplaceMatches(resources, matcher, "vivaldi", exceptions)

  matcher = re.compile(r"http://code.google.com/p/chromium/issues/entry",
                        re.MULTILINE)

  ReplaceMatches(resources, matcher, "https://bugs.vivaldi.com/", exceptions)

def ReplaceGoogleInString(element):
  for exp in GOOGLE_REGEX:
    matcher = re.compile(exp, re.MULTILINE)
    element = matcher.sub("Vivaldi", element)

  matcher = re.compile(r"\bchrome(?=://)", re.MULTILINE)
  element = matcher.sub("vivaldi", element)

  matcher = re.compile(r"http://code.google.com/p/chromium/issues/entry",
                        re.MULTILINE)

  element = matcher.sub("https://bugs.vivaldi.com/", element)
  return element

def update_resources(res, input_file, params={}, allowlist_support=None, active_only=True):

  ReplaceGoogle(res, exceptions=REPLACE_GOOGLE_EXCEPTIONS)

  new_resources = grd_reader.Parse(input_file, **params)

  util.PathSearcher.Configure([
    os.path.dirname(input_file),
    res.GetBaseDir(),
    ])

  new_resources.SetOutputLanguage('en')
  new_resources.SetAllowlistSupportEnabled(allowlist_support)
  new_resources.UberClique().keep_additional_translations_ = True
  new_resources.RunGatherers()

  nodes = new_resources.GetChildrenOfType(empty.OutputsNode)
  if nodes:
    assert(len(nodes) == 1)
    new_node = nodes[0]
    nodes = res.GetChildrenOfType(empty.OutputsNode)
    assert(nodes)
    if nodes:
      assert(len(nodes) == 1)
      old_node = nodes[0]
      old_node.children.extend(new_node.children)

  resource_list={}
  for node in (new_resources.ActiveDescendants()  if active_only else new_resources):
    if "name" not in node.attrs:
      continue
    if isinstance(node, (message.PhNode, message.ExNode)):
      continue
    node.vivaldi_keep_item = True
    resource_list.setdefault(node.attrs["name"], []).append(node)

  names = list(resource_list.keys())

  done_names = set()
  for node in (res.ActiveDescendants()  if active_only else res):
    node_name = node.attrs.get("name", None)
    if not node_name:
      continue
    if isinstance(node, (message.PhNode, message.ExNode)):
      continue

    new_nodes = resource_list.pop(node_name, [])
    if not new_nodes:
      if node_name in done_names:
        assert not active_only, "Multiple active entries for {} in Chromium are not permitted".format(node_name)
        par_node = node.parent
        index = par_node.children.index(node)
        # non-active mode: We already inserted all our entries earlier.
        # Remove all other entries with this name in the upstream resources
        if getattr(par_node.children[index], "vivaldi_keep_item", False):
          continue
        del par_node.children[index]
      continue

    assert len(new_nodes) == 1 or not active_only, "Multiple active entries for {} in Vivaldi are not permitted".format(node_name)
    assert not isinstance(node, message.MessageNode) or getattr(node, "modified_string", False) or any(node.GetCdata() != new_node.GetCdata() for new_node in new_nodes), \
      "Vivaldi Message string {} matches corresponding Chromium string, which is not permitted".format(node_name)

    par_node = node.parent

    try:
      index = par_node.children.index(node)
      par_node.children[index:index+1] = new_nodes
      for new_node in new_nodes:
        new_node.parent = par_node
        for item in new_node.Preorder():
          item.uberclique = res.UberClique()
        for item in new_node.GetCliques():
          item.uberclique = res.UberClique()
    except:
      raise Exception("{} was not registered in parent".format(node_name))
    done_names.add(node_name)

  for name, nodes in resource_list.items():
    for node in nodes:
      if isinstance(node, message.MessageNode):
        par_node = res.GetChildrenOfType(empty.MessagesNode)
        assert par_node, "No Messages node in file for {}".format(name)

      elif isinstance(node, structure.StructureNode):
        par_node = res.GetChildrenOfType(empty.StructuresNode)
        assert par_node, "No Structures node in file for {}".format(name)
      elif isinstance(node, include.IncludeNode):
        par_node = res.GetChildrenOfType(empty.IncludesNode)
        assert par_node, "No Includes node in file for {}".format(name)
      elif isinstance(node, (message.PhNode, message.ExNode)):
        continue
      else:
        raise Exception("Unknown node type {} for {}".format(node, name))

      assert len(par_node) == 1, "There were more than one item of parent type for {}".format(name)
      par_node[0].children.append(node)
      node.parent = par_node[0]
      for item in node.Preorder():
        item.uberclique = res.UberClique()
      for item in node.GetCliques():
        item.uberclique = res.UberClique()
      #print("Inserted {} in {} {}".format(name, node.parent.__class__, node.parent))

  for lang, extra_translation_items in new_resources.UberClique().additional_translations_.items():
    xtb_callback = res.UberClique().GenerateXtbParserCallback(lang, override_exisiting=True, debug = params.get("debug", False))
    for tid, string in extra_translation_items.items():
      #print "Item", tid, type(string), string
      xtb_callback(tid, string);

  return names
