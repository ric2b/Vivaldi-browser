// Copyright (c) 2013-2016 Vivaldi Technologies AS. All rights reserved

#ifndef IMPORTER_VIV_OPERA_READER_H_
#define IMPORTER_VIV_OPERA_READER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/values.h"

class OperaAdrFileReader {
 public:
  bool LoadFile(const base::FilePath& file);

 protected:
  virtual void HandleEntry(const std::string& category,
                           const base::Value::Dict& entries) = 0;

  OperaAdrFileReader();
  virtual ~OperaAdrFileReader();

 private:
  OperaAdrFileReader(const OperaAdrFileReader&) = delete;
  OperaAdrFileReader& operator=(const OperaAdrFileReader&) = delete;
};

#endif  // IMPORTER_VIV_OPERA_READER_H_
