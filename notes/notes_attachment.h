// Copyright (c) 2013-2014 Vivaldi Technologies AS. All rights reserved

#ifndef VIVALDI_NOTES_ENTRY_H_
#define VIVALDI_NOTES_ENTRY_H_

#include <string>
#include <vector>
#include "url/gurl.h"
#include "base/time/time.h"
#include "base/basictypes.h"

namespace base {
  class Value;
  class DictionaryValue;
}

namespace Vivaldi {

struct Notes_attachment
{
  base::string16 filename;

  base::string16 content_type;
  std::string	content;

  base::Value *WriteJSON() const;
  bool ReadJSON(const base::DictionaryValue &);

  bool GetContent(base::string16 *fname,
    base::string16 *cnt_type,
    std::string *cnt);
};
} // namespace Vivaldi
#endif //VIVALDI_NOTES_ENTRY_H_
