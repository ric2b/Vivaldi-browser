// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/signin_partition_manager.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/ui/webui_login_display.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/policy/test/local_policy_test_server.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "media/base/media_switches.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/interfaces/cookie_manager.mojom.h"

namespace em = enterprise_management;

namespace chromeos {

namespace {

constexpr char kTestCookieName[] = "TestCookie";
constexpr char kTestCookieValue[] = "present";
constexpr char kTestCookieHost[] = "host1.com";

void InjectCookieDoneCallback(base::OnceClosure done_closure, bool result) {
  ASSERT_TRUE(result);
  std::move(done_closure).Run();
}

// Injects a cookie into |storage_partition|, so we can test for cookie presence
// later to infer if the StoragePartition has been cleared.
void InjectCookie(content::StoragePartition* storage_partition) {
  network::mojom::CookieManagerPtr cookie_manager;
  storage_partition->GetNetworkContext()->GetCookieManager(
      mojo::MakeRequest(&cookie_manager));

  base::RunLoop run_loop;
  cookie_manager->SetCanonicalCookie(
      net::CanonicalCookie(kTestCookieName, kTestCookieValue, kTestCookieHost,
                           "/", base::Time(), base::Time(), base::Time(), false,
                           false, net::CookieSameSite::NO_RESTRICTION,
                           net::COOKIE_PRIORITY_MEDIUM),
      false, false,
      base::Bind(&InjectCookieDoneCallback, run_loop.QuitClosure()));
  run_loop.Run();
}

void GetAllCookiesCallback(std::string* cookies_out,
                           base::OnceClosure done_closure,
                           const std::vector<net::CanonicalCookie>& cookies) {
  *cookies_out = net::CanonicalCookie::BuildCookieLine(cookies);
  std::move(done_closure).Run();
}

// Returns all cookies present in |storage_partition| as a HTTP header cookie
// line. Will be an empty string if there are no cookies.
std::string GetAllCookies(content::StoragePartition* storage_partition) {
  network::mojom::CookieManagerPtr cookie_manager;
  storage_partition->GetNetworkContext()->GetCookieManager(
      mojo::MakeRequest(&cookie_manager));

  std::string cookies;
  base::RunLoop run_loop;
  cookie_manager->GetAllCookies(
      base::BindOnce(&GetAllCookiesCallback, &cookies, run_loop.QuitClosure()));
  run_loop.Run();
  return cookies;
}

void PolicyChangedCallback(base::RepeatingClosure callback,
                           const base::Value* old_value,
                           const base::Value* new_value) {
  callback.Run();
}

}  // namespace

class WebviewLoginTest : public OobeBaseTest {
 public:
  WebviewLoginTest() {}
  ~WebviewLoginTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kOobeSkipPostLogin);
    command_line->AppendSwitch(::switches::kUseFakeDeviceForMediaStream);
    OobeBaseTest::SetUpCommandLine(command_line);
  }

 protected:
  void ClickNext() {
    ExecuteJsInSigninFrame("document.getElementById('nextButton').click();");
  }

  void ExpectIdentifierPage() {
    // First page: no back button, no close button, refresh button, #identifier
    // input field.
    JsExpect("!$('gaia-navigation').backVisible");
    JsExpect("!$('gaia-navigation').closeVisible");
    JsExpect("$('gaia-navigation').refreshVisible");
    JsExpect("$('signin-frame').src.indexOf('#identifier') != -1");
  }

  void ExpectPasswordPage() {
    // Second page: back button, close button, no refresh button,
    // #challengepassword input field.
    JsExpect("$('gaia-navigation').backVisible");
    JsExpect("$('gaia-navigation').closeVisible");
    JsExpect("!$('gaia-navigation').refreshVisible");
    JsExpect("$('signin-frame').src.indexOf('#challengepassword') != -1");
  }

  bool WebViewVisited(content::BrowserContext* browser_context,
                      content::StoragePartition* expected_storage_partition,
                      bool* out_web_view_found,
                      content::WebContents* guest_contents) {
    content::StoragePartition* guest_storage_partition =
        content::BrowserContext::GetStoragePartition(
            browser_context, guest_contents->GetSiteInstance());
    if (guest_storage_partition == expected_storage_partition)
      *out_web_view_found = true;

    // Returns true if found - this will exit the iteration early.
    return *out_web_view_found;
  }

  // Returns true if a webview which has a WebContents associated with
  // |storage_partition| currently exists in the login UI's main WebContents.
  bool IsLoginScreenHasWebviewWithStoragePartition(
      content::StoragePartition* storage_partition) {
    bool web_view_found = false;

    content::WebContents* web_contents = GetLoginUI()->GetWebContents();
    content::BrowserContext* browser_context =
        web_contents->GetBrowserContext();
    guest_view::GuestViewManager* guest_view_manager =
        guest_view::GuestViewManager::FromBrowserContext(browser_context);
    guest_view_manager->ForEachGuest(
        web_contents,
        base::Bind(&WebviewLoginTest::WebViewVisited, base::Unretained(this),
                   browser_context, storage_partition, &web_view_found));

    return web_view_found;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebviewLoginTest);
};

// Basic signin with username and password.
IN_PROC_BROWSER_TEST_F(WebviewLoginTest, Basic) {
  WaitForGaiaPageLoad();

  ExpectIdentifierPage();

  SetSignFormField("identifier", OobeBaseTest::kFakeUserEmail);
  ClickNext();
  ExpectPasswordPage();

  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());

  SetSignFormField("password", OobeBaseTest::kFakeUserPassword);
  ClickNext();

  session_start_waiter.Wait();
}

// Fails: http://crbug.com/512648.
IN_PROC_BROWSER_TEST_F(WebviewLoginTest, DISABLED_BackButton) {
  WaitForGaiaPageLoad();

  // Start with identifer page.
  ExpectIdentifierPage();

  // Move to password page.
  SetSignFormField("identifier", OobeBaseTest::kFakeUserEmail);
  ClickNext();
  ExpectPasswordPage();

  // Click back to identifier page.
  JS().Evaluate("$('gaia-navigation').$.backButton.click();");
  ExpectIdentifierPage();

  // Click next to password page, user id is remembered.
  ClickNext();
  ExpectPasswordPage();

  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());

  // Finish sign-up.
  SetSignFormField("password", OobeBaseTest::kFakeUserPassword);
  ClickNext();

  session_start_waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(WebviewLoginTest, AllowGuest) {
  WaitForGaiaPageLoad();
  JsExpect("!$('guest-user-header-bar-item').hidden");
  CrosSettings::Get()->SetBoolean(kAccountsPrefAllowGuest, false);
  JsExpect("$('guest-user-header-bar-item').hidden");
}

// Create new account option should be available only if the settings allow it.
IN_PROC_BROWSER_TEST_F(WebviewLoginTest, AllowNewUser) {
  WaitForGaiaPageLoad();

  std::string frame_url = "$('gaia-signin').gaiaAuthHost_.reloadUrl_";
  // New users are allowed.
  JsExpect(frame_url + ".search('flow=nosignup') == -1");

  // Disallow new users - we also need to set a whitelist due to weird logic.
  CrosSettings::Get()->Set(kAccountsPrefUsers, base::ListValue());
  CrosSettings::Get()->SetBoolean(kAccountsPrefAllowNewUser, false);
  WaitForGaiaPageReload();

  // flow=nosignup indicates that user creation is not allowed.
  JsExpect(frame_url + ".search('flow=nosignup') != -1");
}

IN_PROC_BROWSER_TEST_F(WebviewLoginTest, EmailPrefill) {
  WaitForGaiaPageLoad();
  JS().ExecuteAsync("Oobe.showSigninUI('user@example.com')");
  WaitForGaiaPageReload();
  EXPECT_EQ(fake_gaia_->prefilled_email(), "user@example.com");
}

IN_PROC_BROWSER_TEST_F(WebviewLoginTest, StoragePartitionHandling) {
  WaitForGaiaPageLoad();

  // Start with identifer page.
  ExpectIdentifierPage();

  // WebContents of the embedding frame
  content::WebContents* web_contents = GetLoginUI()->GetWebContents();
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();

  std::string signin_frame_partition_name_1 =
      JS().GetString("$('signin-frame').partition");
  content::StoragePartition* signin_frame_partition_1 =
      login::GetSigninPartition();

  EXPECT_FALSE(signin_frame_partition_name_1.empty());
  EXPECT_EQ(login::SigninPartitionManager::Factory::GetForBrowserContext(
                browser_context)
                ->GetCurrentStoragePartitionName(),
            signin_frame_partition_name_1);
  EXPECT_TRUE(
      IsLoginScreenHasWebviewWithStoragePartition(signin_frame_partition_1));
  // Inject a cookie into the currently used StoragePartition, so we can test
  // later if it has been cleared.
  InjectCookie(signin_frame_partition_1);

  // Press the back button at a sign-in screen without pre-existing users to
  // start a new sign-in attempt.
  JS().Evaluate("$('signin-back-button').fire('tap')");
  WaitForGaiaPageReload();
  // Expect that we got back to the identifier page, as there are no known users
  // so the sign-in screen will not display user pods.
  ExpectIdentifierPage();

  std::string signin_frame_partition_name_2 =
      JS().GetString("$('signin-frame').partition");
  content::StoragePartition* signin_frame_partition_2 =
      login::GetSigninPartition();

  EXPECT_FALSE(signin_frame_partition_name_2.empty());
  EXPECT_EQ(login::SigninPartitionManager::Factory::GetForBrowserContext(
                browser_context)
                ->GetCurrentStoragePartitionName(),
            signin_frame_partition_name_2);
  EXPECT_TRUE(
      IsLoginScreenHasWebviewWithStoragePartition(signin_frame_partition_2));
  InjectCookie(signin_frame_partition_2);

  // Make sure that the partitions differ and that the old one is not in use
  // anymore.
  EXPECT_NE(signin_frame_partition_name_1, signin_frame_partition_name_2);
  EXPECT_NE(signin_frame_partition_1, signin_frame_partition_2);
  EXPECT_FALSE(
      IsLoginScreenHasWebviewWithStoragePartition(signin_frame_partition_1));

  // The StoragePartition which is not in use is supposed to have been cleared.
  EXPECT_EQ("", GetAllCookies(signin_frame_partition_1));
  EXPECT_NE("", GetAllCookies(signin_frame_partition_2));
}

// Tests that requesting webcam access from the login screen works correctly.
// This is needed for taking profile pictures.
IN_PROC_BROWSER_TEST_F(WebviewLoginTest, RequestCamera) {
  WaitForGaiaPageLoad();

  // Video devices should be allowed from the login screen.
  content::WebContents* web_contents = GetLoginUI()->GetWebContents();
  bool getUserMediaSuccess = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents->GetMainFrame(),
      "navigator.getUserMedia("
      "    {video: true},"
      "    function() { window.domAutomationController.send(true); },"
      "    function() { window.domAutomationController.send(false); });",
      &getUserMediaSuccess));
  EXPECT_TRUE(getUserMediaSuccess);

  // Audio devices should be denied from the login screen.
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents->GetMainFrame(),
      "navigator.getUserMedia("
      "    {audio: true},"
      "    function() { window.domAutomationController.send(true); },"
      "    function() { window.domAutomationController.send(false); });",
      &getUserMediaSuccess));
  EXPECT_FALSE(getUserMediaSuccess);
}

class WebviewProxyAuthLoginTest : public WebviewLoginTest {
 public:
  WebviewProxyAuthLoginTest()
      : auth_proxy_server_(std::make_unique<net::SpawnedTestServer>(
            net::SpawnedTestServer::TYPE_BASIC_AUTH_PROXY,
            base::FilePath())) {}

 protected:
  void SetUp() override {
    // Start proxy server
    auth_proxy_server_->set_redirect_connect_to_localhost(true);
    ASSERT_TRUE(auth_proxy_server_->Start());

    // Prepare device policy which will be used for two purposes:
    // - given to |fake_session_manager_client_|, so the device appears to have
    //   registered for policy.
    // - the payload is given to |policy_test_server_|, so we can download fresh
    //   policy.
    device_policy_test_helper_.device_policy()
        ->policy_data()
        .set_public_key_version(1);
    device_policy_test_helper_.device_policy()->Build();

    // Start policy server. Use the DMToken and DeviceId from PolicyBuilder.
    // These also used in |device_policy_test_helper_| and was passed to
    // |fake_session_manager_client_| above, so the device will request policy
    // with these identifiers.
    policy_test_server_.RegisterClient(policy::PolicyBuilder::kFakeToken,
                                       policy::PolicyBuilder::kFakeDeviceId);
    UpdateServedPolicyFromDevicePolicyTestHelper();
    ASSERT_TRUE(policy_test_server_.Start());

    WebviewLoginTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        ::switches::kProxyServer,
        auth_proxy_server_->host_port_pair().ToString());
    command_line->AppendSwitchASCII(policy::switches::kDeviceManagementUrl,
                                    policy_test_server_.GetServiceURL().spec());
    WebviewLoginTest::SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    WebviewLoginTest::SetUpInProcessBrowserTestFixture();

    // Use a fake SessionManagerClient to be able to pretend that the device has
    // been enrolled and registered for policy (and has a device DMToken).
    auto fake_session_manager_client =
        std::make_unique<FakeSessionManagerClient>();
    fake_session_manager_client_ = fake_session_manager_client.get();
    DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        std::move(fake_session_manager_client));
    device_policy_test_helper_.InstallOwnerKey();
    device_policy_test_helper_.MarkAsEnterpriseOwned();

    fake_session_manager_client_->set_device_policy(
        device_policy_builder()->GetBlob());

    // Set some fake state keys to make sure they are not empty.
    std::vector<std::string> state_keys;
    state_keys.push_back("1");
    fake_session_manager_client_->set_server_backed_state_keys(state_keys);
  }

  void SetUpOnMainThread() override {
    // Setup the observer reacting on NOTIFICATION_AUTH_NEEDED before the test
    // runs because there is no action we actively trigger to request proxy
    // authentication. Instead, the sign-in screen automatically shows the gaia
    // webview, which will request the gaia URL, which leads to a login prompt.
    auth_needed_wait_loop_ = std::make_unique<base::RunLoop>();
    auth_needed_observer_ =
        std::make_unique<content::WindowedNotificationObserver>(
            chrome::NOTIFICATION_AUTH_NEEDED,
            base::BindRepeating(&WebviewProxyAuthLoginTest::OnAuthRequested,
                                base::Unretained(this)));

    WebviewLoginTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    WebviewLoginTest::TearDownOnMainThread();

    auth_needed_observer_.reset();
    auth_needed_wait_loop_.reset();
  }

  bool OnAuthRequested(const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
    // Only care for notifications originating from the frame which is
    // displaying gaia.
    content::WebContents* main_web_contents = GetLoginUI()->GetWebContents();
    content::WebContents* gaia_frame_web_contents =
        signin::GetAuthFrameWebContents(main_web_contents, gaia_frame_parent_);
    LoginHandler* login_handler =
        content::Details<LoginNotificationDetails>(details)->handler();
    if (login_handler->GetWebContentsForLogin() != gaia_frame_web_contents)
      return false;

    gaia_frame_login_handler_ = login_handler;
    auth_needed_wait_loop_->Quit();
    return true;
  }

  // Waits until proxy authentication has been requested by the frame displaying
  // gaia. Returns the LoginHandler handling this authentication request.
  LoginHandler* WaitForAuthRequested() {
    auth_needed_wait_loop_->Run();
    return gaia_frame_login_handler_;
  }

  void UpdateServedPolicyFromDevicePolicyTestHelper() {
    policy_test_server_.UpdatePolicy(
        policy::dm_protocol::kChromeDevicePolicyType,
        std::string() /* entity_id */,
        device_policy_builder()->payload().SerializeAsString());
  }

  policy::DevicePolicyBuilder* device_policy_builder() {
    return device_policy_test_helper_.device_policy();
  }

  content::WindowedNotificationObserver* auth_needed_observer() {
    return auth_needed_observer_.get();
  }

 private:
  std::unique_ptr<content::WindowedNotificationObserver> auth_needed_observer_;
  std::unique_ptr<base::RunLoop> auth_needed_wait_loop_;
  // Unowned pointer - set to the LoginHandler of the frame displaying gaia.
  LoginHandler* gaia_frame_login_handler_ = nullptr;

  // A proxy server which requires authentication using the 'Basic'
  // authentication method.
  std::unique_ptr<net::SpawnedTestServer> auth_proxy_server_;
  policy::LocalPolicyTestServer policy_test_server_;
  policy::DevicePolicyCrosTestHelper device_policy_test_helper_;

  // FakeDBusThreadManager uses FakeSessionManagerClient.
  std::unique_ptr<chromeos::DBusThreadManagerSetter> dbus_setter_;
  // Unowned pointer - owned by DBusThreadManager.
  chromeos::FakeSessionManagerClient* fake_session_manager_client_;

  DISALLOW_COPY_AND_ASSIGN(WebviewProxyAuthLoginTest);
};

IN_PROC_BROWSER_TEST_F(WebviewProxyAuthLoginTest, ProxyAuthTransfer) {
  WaitForSigninScreen();

  LoginHandler* login_handler = WaitForAuthRequested();

  // Before entering auth data, make |policy_test_server_| serve a policy that
  // we can use to detect if policies have been fetched.
  em::ChromeDeviceSettingsProto& device_policy =
      device_policy_builder()->payload();
  device_policy.mutable_device_policy_refresh_rate()
      ->set_device_policy_refresh_rate(10000);
  UpdateServedPolicyFromDevicePolicyTestHelper();

  policy::PolicyChangeRegistrar policy_change_registrar(
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetPolicyService(),
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                              std::string() /* component_id */));

  // Now enter auth data
  login_handler->SetAuth(base::ASCIIToUTF16("foo"), base::ASCIIToUTF16("bar"));
  WaitForGaiaPageLoad();

  base::RunLoop run_loop;
  policy_change_registrar.Observe(
      policy::key::kDevicePolicyRefreshRate,
      base::BindRepeating(&PolicyChangedCallback, run_loop.QuitClosure()));
  run_loop.Run();

  // Press the back button at a sign-in screen without pre-existing users to
  // start a new sign-in attempt.
  // This will re-load gaia, rotating the StoragePartition. The new
  // StoragePartition must also have the proxy auth details.
  JS().Evaluate("$('signin-back-button').fire('tap')");
  WaitForGaiaPageReload();
  // Expect that we got back to the identifier page, as there are no known users
  // so the sign-in screen will not display user pods.
  ExpectIdentifierPage();
}
}  // namespace chromeos
