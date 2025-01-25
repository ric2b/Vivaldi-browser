// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/ash_browser_test_starter.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/containers/to_vector.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/test/test_switches.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_manager_observer.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "chromeos/ash/components/standalone_browser/test_util.h"
#include "components/exo/wm_helper.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/user_manager/fake_device_ownership_waiter.h"
#include "content/public/common/content_switches.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/views/views_switches.h"

namespace test {

namespace {

using ::net::test_server::HungResponse;

std::unique_ptr<net::test_server::HttpResponse> HandleGaiaURL(
    const GURL& base_url,
    const net::test_server::HttpRequest& request) {
  // Simulate failure for Gaia url request.
  return std::make_unique<HungResponse>();
}

class NewLacrosWindowWatcher : public exo::WMHelper::ExoWindowObserver {
 public:
  NewLacrosWindowWatcher() {
    exo::WMHelper::GetInstance()->AddExoWindowObserver(this);
  }

  ~NewLacrosWindowWatcher() override {
    exo::WMHelper::GetInstance()->RemoveExoWindowObserver(this);
  }

  NewLacrosWindowWatcher(const NewLacrosWindowWatcher&) = delete;
  NewLacrosWindowWatcher& operator=(const NewLacrosWindowWatcher&) = delete;

  // `exo::WMHelper::ExoWindowObserver`
  void OnExoWindowCreated(aura::Window* window) override {
    if (crosapi::browser_util::IsLacrosWindow(window)) {
      window_future_.SetValue(window);
    }
  }

  // Waits until a new Lacros window is created.
  // The watch period starts when this object was created, not when this method
  // is called. In other words, this method may return immediately if a Lacros
  // window was already created before.
  aura::Window* Await() { return window_future_.Take(); }

 private:
  base::test::TestFuture<aura::Window*> window_future_;
};

}  // namespace

AshBrowserTestStarter::~AshBrowserTestStarter() {
  CHECK(!initial_lacros_window_);  // Exo has already shut down.

  // Clean up the directories that tests were passed. This is
  // to save bot collect results time and faster CQ runtime.
  if (!ash_user_data_dir_for_cleanup_.empty() &&
      !::testing::Test::HasFailure() &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTestLauncherBotMode)) {
    // Intentionally not check return value.
    base::DeletePathRecursively(ash_user_data_dir_for_cleanup_);
  }
}

AshBrowserTestStarter::AshBrowserTestStarter()
    : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
  https_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleGaiaURL, base_url()));

  bool success = https_server()->InitializeAndListen();
  CHECK(success);
  https_server()->StartAcceptingConnections();
}

bool AshBrowserTestStarter::HasLacrosArgument() const {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kLacrosChromePath);
}

net::EmbeddedTestServer* AshBrowserTestStarter::https_server() {
  return &https_server_;
}

GURL AshBrowserTestStarter::base_url() {
  return https_server()->base_url();
}

bool AshBrowserTestStarter::PrepareEnvironmentForLacros() {
  DCHECK(HasLacrosArgument());
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  if (!scoped_temp_dir_xdg_.CreateUniqueTempDir()) {
    return false;
  }
  env->SetVar("XDG_RUNTIME_DIR", scoped_temp_dir_xdg_.GetPath().AsUTF8Unsafe());

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // Put lacros logs in CAS outputs on bots.
  if (command_line->HasSwitch(switches::kTestLauncherSummaryOutput)) {
    std::string test_name = base::JoinString(
        {::testing::UnitTest::GetInstance()
             ->current_test_info()
             ->test_suite_name(),
         ::testing::UnitTest::GetInstance()->current_test_info()->name()},
        ".");
    base::FilePath output_file_path =
        command_line->GetSwitchValuePath(switches::kTestLauncherSummaryOutput);
    base::FilePath test_output_folder =
        output_file_path.DirName().Append(test_name);
    // Need to create a unique user-data-dir across all test runners.
    // Using test name as folder name is usually enough. But there are edge
    // cases it needs to handle:
    //  * Test retry. On bots, usually we retry 1 time for test failures.
    //  * Flakiness endorser. It will run new tests 20 times to ensure it's not
    //    flaky.
    //  * Developer modified builder config. Developer may modify infra to run
    //    a test multiple times(e.g. 100) on bot to check flakiness.
    // To handle those edge cases, we would create the user-data-dir using some
    // random numbers to ensure it's unique.
    int retry_count = 1;
    bool success = false;
    while (!success && retry_count < 100) {
      if (base::PathExists(test_output_folder)) {
        test_output_folder = output_file_path.DirName().Append(
            (test_name + ".attempt_" +
             base::NumberToString(base::RandInt(0, 10000))));
      } else {
        success = base::CreateDirectory(test_output_folder);
      }
      ++retry_count;
    }

    // If we didn't create a unique directory, we would let
    // the browser test framework to create the tmp folder as usual.
    if (success) {
      command_line->AppendSwitchPath(switches::kUserDataDir,
                                     test_output_folder);
      ash_user_data_dir_for_cleanup_ = test_output_folder;
    }
  } else {
    LOG(WARNING)
        << "By default, lacros logs are in some random folder. If you need "
        << "lacros log, please run with --test-launcher-summary-output. e.g. "
        << "Run with --test-launcher-summary-output=/tmp/default/output.json. "
        << "For each lacros in a test, the log will be in "
        << "/tmp/default/test_suite.test_name folder.";
  }

  scoped_feature_list_.InitWithFeatures(
      ash::standalone_browser::GetFeatureRefs(), {});
  command_line->AppendSwitch(ash::switches::kEnableLacrosForTesting);
  command_line->AppendSwitch(ash::switches::kAshEnableWaylandServer);
  command_line->AppendSwitch(
      views::switches::kDisableInputEventActivationProtectionForTesting);
  command_line->AppendSwitch(ash::switches::kDisableLacrosKeepAliveForTesting);
  command_line->AppendSwitch(ash::switches::kDisableLoginLacrosOpening);
  command_line->AppendSwitch(switches::kNoStartupWindow);

  std::vector<std::string> lacros_args;
  lacros_args.emplace_back(base::StringPrintf("--%s", switches::kNoFirstRun));
  lacros_args.emplace_back(
      base::StringPrintf("--%s", switches::kIgnoreCertificateErrors));
  // For some StandaloneBrowserTestController features.
  lacros_args.emplace_back(
      base::StringPrintf("--%s", switches::kDomAutomationController));
  // Override Gaia url in Lacros so that the gaia requests will NOT be handled
  // with the real internet connection, but with the embedded test server. The
  // embedded test server will simulate failure of the Gaia url requests which
  // is expected in testing environment for Gaia authentication flow. This is a
  // workaround for fixing crbug/1371655.
  lacros_args.emplace_back(base::StringPrintf("--%s=%s", switches::kGaiaUrl,
                                              base_url().spec().c_str()));
  // Disable gpu process in Lacros since hardware accelerated rendering is
  // not possible yet in Ash X11 backend. See details in crbug/1478369.
  lacros_args.emplace_back("--disable-gpu");
  // Disable gpu sandbox in Lacros since it fails in Linux emulator environment.
  // See details in crbug/1483530.
  lacros_args.emplace_back("--disable-gpu-sandbox");
  ash::standalone_browser::AddLacrosArguments(lacros_args, command_line);

  return true;
}

void AshBrowserTestStarter::EnableFeaturesInLacros(
    const std::vector<base::test::FeatureRef>& features) {
  CHECK(HasLacrosArgument());

  std::vector<std::string> feature_strings = base::ToVector(  // IN-TEST
      features, [](base::test::FeatureRef feature) -> std::string {
        return feature->name;
      });

  std::string features_arg =
      "--enable-features=" + base::JoinString(feature_strings, ",");
  std::vector<std::string> lacros_args = {features_arg};
  ash::standalone_browser::AddLacrosArguments(
      lacros_args, base::CommandLine::ForCurrentProcess());
}

void AshBrowserTestStarter::StartLacros(InProcessBrowserTest* test_class_obj) {
  DCHECK(HasLacrosArgument());

  SetUpBrowserManager();

  {
    NewLacrosWindowWatcher watcher;
    crosapi::BrowserManager::Get()->NewWindow(
        /*incongnito=*/false, /*should_trigger_session_restore=*/false);
    initial_lacros_window_ = watcher.Await();
  }
  initial_lacros_window_->AddObserver(this);  // For OnWindowDestroying.

  CHECK(crosapi::BrowserManager::Get()->IsRunning());
}

void AshBrowserTestStarter::SetUpBrowserManager() {
  DCHECK(HasLacrosArgument());

  crosapi::BrowserManager::Get()->set_device_ownership_waiter_for_testing(
      std::make_unique<user_manager::FakeDeviceOwnershipWaiter>());
}

void AshBrowserTestStarter::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window, initial_lacros_window_);
  initial_lacros_window_ = nullptr;
}

}  // namespace test
