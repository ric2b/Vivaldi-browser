// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_EDITCOMMAND_EDITCOMMAND_API_H_
#define EXTENSIONS_API_EDITCOMMAND_EDITCOMMAND_API_H_

#include "extensions/browser/extension_function.h"
#include "extensions/schema/editcommand.h"

namespace extensions {

class EditcommandExecuteFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("editcommand.execute", EDITCOMMAND_EXECUTE)
  EditcommandExecuteFunction() = default;

 private:
  ~EditcommandExecuteFunction() override = default;
  ResponseAction Run() override;
  DISALLOW_COPY_AND_ASSIGN(EditcommandExecuteFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_EDITCOMMAND_EDITCOMMAND_API_H_
