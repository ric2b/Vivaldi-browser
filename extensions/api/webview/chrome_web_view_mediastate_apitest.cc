// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "base/path_service.h"
#include "base/vivaldi_paths.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"

using base::PathService;

namespace extensions {

#if 0
class VivaldiExtensionApiTest : public ExtensionApiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    vivaldi::RegisterVivaldiPaths();
    ExtensionApiTest::SetUpCommandLine(command_line);

    PathService::Get(vivaldi::DIR_VIVALDI_TEST_DATA, &test_data_dir_);
    test_data_dir_ = test_data_dir_.AppendASCII("extensions");
  }
};

// Testing 'webview.onMediaStateChanged.'
// tomas@vivaldi.com - disabled test (VB-7468)
IN_PROC_BROWSER_TEST_F(VivaldiExtensionApiTest, DISABLED_WebviewMediastate) {
  ASSERT_TRUE(RunPlatformAppTest("webview/mediastate_event")) << message_;
}
#endif

}  // namespace extensions
