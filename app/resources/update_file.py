# Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

import os, sys
import argparse
import json
import shutil
import refresh_file

def ScanDir(scandir):
  dirlist = []
  for dirname, _dirs, filenames in os.walk(scandir):
    dirname = os.path.normpath(os.path.relpath(dirname, scandir))
    if dirname == ".":
      dirname = None
    else:
      assert dirname and not dirname.startswith("."),  dirname
    dirlist.extend([os.path.join(
                      *filter(None, [dirname, f])).replace("\\", "/")
                        for f in filenames])
  return dirlist


class UpdateConfig:

  def __init__(self, json_name, target_name, target_dir, root_dir):
    self.json_time = os.path.getmtime(json_name)
    self.update_json = json.load(open(json_name, "r"))
    self.target_name = target_name
    self.target_dir = target_dir
    self.root_dir = root_dir
    assert isinstance(self.update_json, dict)


  def __get_section(self, section):
    updates = self.update_json.get(section, {})
    update_list = dict([(x,y) for x, y in updates.get("MANY", {}).iteritems()
                        if self.target_name in y.get("targets", [])])
    update_list.update(updates.get(self.target_name, {}))
    return update_list

  def __get_target_filename(self, org_file, fp, config):
    if "basepath" in config:
      base_path = config["basepath"]
      if org_file:
        base_path = os.path.join(self.root_dir, base_path)
      else:
        org_file = fp
      target_filename = os.path.relpath(org_file,base_path)
    else:
      target_filename = config.get("destination",os.path.basename(fp))

    target_file = os.path.normpath(os.path.join(self.target_dir,
                                                target_filename))

    return target_file

  def __conditional_action(self, fp, config, action, force=False):
    assert callable(action)

    config = config or {}
    source_file = os.path.normpath(os.path.join(self.root_dir,fp))
    target_file = self.__get_target_filename(source_file, fp, config)

    if not force:
      org_time = os.path.getmtime(source_file)
      try:
        target_time = os.path.getmtime(target_file)
        if org_time < target_time and self.json_time>=target_time:
          return
      except:
        pass # ignore, run operation

    action(source_file, target_file, config)

  def update_files(self, force=False):
    def update_file(fp, config):
      def perform_update(source_file, target_file, config):
        #print >>sys.stderr, "Updating", source_file
        content = open(source_file,"r").read()
        encoding = config.get("encoding", None)
        if encoding:
          content = content.decode(encoding)
        if config and "stringreplace" in config:
          for match in config["stringreplace"]:
            pattern = match.get("match",None)
            #if "targets" in config:
            #  raise Exception(fp+":"+str(config["targets"]))
            #print >>sys.stderr, "Replacing |"+pattern+"| with |"+ match.get("replace", "") +"|"
            if not pattern:
              continue
            oldcontent = content
            content = content.replace(pattern, match.get("replace", ""))
            if content == oldcontent:
              raise Exception("No changes in "+fp+":"+pattern)

        if encoding:
          content = content.encode(encoding)

        #print >>sys.stderr, "="*40
        #print >>sys.stderr, content
        #print >>sys.stderr, "="*40
        #print >>sys.stderr, target_file
        try:
          os.makedirs(os.path.dirname(target_file))
        except:
          pass
        refresh_file.conditional_refresh_file(target_file, content)
      self.__conditional_action(fp, config, perform_update, force=force)

    update_list = self.__get_section("updates")
    for fp, config in update_list.iteritems():
      update_file(fp, config)

    update_list = self.__get_section("extra_updates")
    for fp, config in update_list.iteritems():
      update_file(fp, config)

  def get_update_sources(self):
    update_list = self.__get_section("updates")
    source = list(update_list.iterkeys())

    update_list = self.__get_section("extra_updates")
    source.extend(list(update_list.iterkeys()))

    return [os.path.join(self.root_dir,fp).replace("\\", "/") for fp in source]

  def get_update_targets(self):
    source = []

    update_list = self.__get_section("updates")
    for fp, config in update_list.iteritems():
      target_file = self.__get_target_filename(None, fp, config)
      source.append(target_file.replace("\\", "/"))

    update_list = self.__get_section("extra_updates")
    for fp, config in update_list.iteritems():
      target_file = self.__get_target_filename(None, fp, config)
      source.append(target_file.replace("\\", "/"))

    return source

  def __get_dir_list(self, fp, config):
    if config.get("recursive",False):
      dirlist = ScanDir(os.path.join(self.root_dir,fp))
      return [os.path.join(fp ,x) for x in dirlist]
    else:
      return [fp]

  def copy_files(self, force=False):
    def copy_file(fp, config):
      def perform_copy(source_file, target_file, config):
        try:
          os.makedirs(os.path.dirname(target_file))
        except:
          pass
        refresh_file.conditional_copy(source_file, target_file)
      self.__conditional_action(fp, config, perform_copy, force=force)

    update_list = self.__get_section("copies")
    for fp, config in update_list.iteritems():
      dirlist = self.__get_dir_list(fp, config)

      for x in dirlist:
        try:
          copy_file(x, config)
        except:
          print >>sys.stderr, self.root_dir, self.target_dir, fp, str(config), x
          raise

  def get_copy_sources(self):
    update_list = self.__get_section("copies")
    source = []
    for fp, config in update_list.iteritems():
      source.extend(self.__get_dir_list(fp, config))

    return [os.path.join(self.root_dir,fp).replace("\\", "/") for fp in source]

  def get_copy_targets(self):
    update_list = self.__get_section("copies")
    source = []
    for fp, config in update_list.iteritems():
      dirlist = self.__get_dir_list(fp, config)

      for fp1 in dirlist:
        target_file = self.__get_target_filename(None, fp1, config)
        source.append(target_file)

    return source

def main():
  argparser = argparse.ArgumentParser();

  argparser.add_argument("-f","--force", action="store_true")
  argparser.add_argument("json")
  argparser.add_argument("target_name")
  argparser.add_argument("target_dir")
  argparser.add_argument("root_dir")

  options = argparser.parse_args()

  config = UpdateConfig(options.json, options.target_name, options.target_dir,
                        options.root_dir)

  config.update_files(force=options.force)
  config.copy_files(force=options.force)

if __name__ == '__main__':
    main()