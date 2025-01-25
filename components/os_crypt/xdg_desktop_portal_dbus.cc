// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "components/os_crypt/xdg_desktop_portal_dbus.h"

#include <unistd.h>

#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"

namespace {

constexpr char kPortalDesktopBusName[] = "org.freedesktop.portal.Desktop";
constexpr char kPortalSecretBusName[] = "org.freedesktop.portal.Secret";
constexpr char kPortalRequestBusName[] = "org.freedesktop.portal.Request";

constexpr char kPortalDesktopObjectPath[] = "/org/freedesktop/portal/desktop";
constexpr char kPortalDesktopRequestObjectPath[] = "/org/freedesktop/portal/desktop/request/";

// receive timeout for secret, then we report that we can't receive
const unsigned kSecretReceiveTimeout = 5;

std::string ConvertNameToPathElement(const std::string& name) {
  std::string formatted_name = name.substr(1);  // Remove ':'
  base::ReplaceChars(formatted_name, ".", "_", &formatted_name);
  return formatted_name;
}

}  // namespace

SecretPortalDBus::SecretPortalDBus() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SESSION;
  options.connection_type = dbus::Bus::PRIVATE;
  bus_ = base::MakeRefCounted<dbus::Bus>(options);
}

SecretPortalDBus::~SecretPortalDBus() {
  bus_->ShutdownAndBlock();
}

bool SecretPortalDBus::RetrieveSecret(std::optional<std::string> token,
                                      std::string* output) {
  if (!bus_->Connect()) {
    LOG(ERROR)
        << "Xdg-Desktop-Portal: Failed to connect to the D-Bus session bus";
    return false;
  }

  // Create a pipe for the key to be transferred through.
  base::ScopedFD read_fd;
  base::ScopedFD write_fd;
  if (!base::CreatePipe(&read_fd, &write_fd, true)) {
    LOG(ERROR) << "Xdg-Desktop-Portal: Failed to create pipe";
    return false;
  }
  secret_reader_ = std::make_unique<base::File>(std::move(read_fd));

  // Proxy to the desktop portal.
  scoped_refptr<dbus::ObjectProxy> proxy =
      bus_->GetObjectProxy(kPortalDesktopBusName,
                           dbus::ObjectPath{kPortalDesktopObjectPath});

  // We generate a random handle token (non-guessable) for the Request object
  // path.
  std::string handle_token = base::NumberToString(base::RandUint64());

  // To avoid race condition (signal fired before we connect to it)
  // we connect to the object's signal before making the call.
  std::string client_name_element =
      ConvertNameToPathElement(bus_->GetConnectionName());
  auto advised_request_path =
      dbus::ObjectPath{kPortalDesktopRequestObjectPath +
                       client_name_element + "/" + handle_token};

  scoped_refptr<dbus::ObjectProxy> request_proxy = bus_->GetObjectProxy(
      kPortalDesktopBusName, advised_request_path);

  // This will be used to block until we get something out of the portal (or
  // timeout).
  run_loop_ = std::make_unique<base::RunLoop>();

  request_proxy->ConnectToSignal(
      kPortalRequestBusName, "Response",
      // On Signal
      base::BindRepeating(&SecretPortalDBus::OnResponse,
                          base::Unretained(this)),
      // On Connected
      base::BindOnce(&SecretPortalDBus::OnConnected, base::Unretained(this)));

  // The environment is prepared. Now call the Method.
  dbus::MethodCall method_call(kPortalSecretBusName,
                               "RetrieveSecret");

  dbus::MessageWriter writer(&method_call);
  writer.AppendFileDescriptor(write_fd.get());

  dbus::MessageWriter options_writer(nullptr);
  writer.OpenArray("{sv}", &options_writer);

  // Add the token to the options dictionary if provided
  if (token) {
    dbus::MessageWriter entry_writer(nullptr);
    options_writer.OpenDictEntry(&entry_writer);
    entry_writer.AppendString("token");
    entry_writer.AppendVariantOfString(*token);
    options_writer.CloseContainer(&entry_writer);
  }

  // Unique handle_token is used to let us subscribe to signal before the call
  // is done.
  dbus::MessageWriter entry_writer(nullptr);
  options_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString("handle_token");
  entry_writer.AppendVariantOfString(handle_token);
  options_writer.CloseContainer(&entry_writer);

  writer.CloseContainer(&options_writer);

  auto response = proxy->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);

  if (!response.has_value()) {
    LOG(ERROR) << "Xdg-Desktop-Portal: Failed to call secret portal: "
               << response.error().message();
    return false;
  }
  dbus::MessageReader reader(response.value().get());

  dbus::ObjectPath request_path;
  if (!reader.PopObjectPath(&request_path)) {
    LOG(ERROR)
        << "Xdg-Desktop-Portal: Failed to get request path from response";
    return false;
  }

  if (request_path != advised_request_path) {
    LOG(ERROR) << "Xdg-Desktop-Portal: Mismatch in request path. Can't wait "
                  "for the signal.";
    return false;
  }

  // Set a timeout of 5s to avoid stalling indefinitely in case we don't receive
  // the signal.
  base::TimeDelta timeout = base::Seconds(kSecretReceiveTimeout);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop_->QuitClosure(), timeout);

  // Will get terminated in the signal handler
  run_loop_->Run();

  // Secret was read from the PIPE, we have a result.
  if (!secret_.empty() && output) {
    *output = secret_;
    return true;
  }

  LOG(ERROR) << "Xdg-Desktop-Portal: Could not retrieve the secret from the "
                "secret portal (Other error or timeout).";
  return false;  // some other error?
}

void SecretPortalDBus::OnResponse(dbus::Signal* signal) {
  dbus::MessageReader reader(signal);
  uint32_t response_code;
  if (!reader.PopUint32(&response_code)) {
    LOG(ERROR) << "Failed to read response code";
    run_loop_->Quit();
    return;
  }

  if (response_code != 0) {
    LOG(ERROR)
        << "Xdg-Desktop-Portal: Portal request failed with response code: "
        << response_code << " " << signal->ToString();
    run_loop_->Quit();
    return;
  }

  std::string secret;
  char buf[512];

  int n = secret_reader_->ReadAtCurrentPos(buf, sizeof(buf));

  if (n <= 0 || static_cast<size_t>(n) >= sizeof(buf)) {
    LOG(ERROR) << "Xdg-Desktop-Portal: Got an invalid secret result when "
                  "reading secret "
                  "from portal. Secret length "
               << n;
    run_loop_->Quit();
    return;
  }

  secret_ = std::string(buf, n);
  run_loop_->Quit();
}

void SecretPortalDBus::OnConnected(const std::string& interface,
                                   const std::string& signal,
                                   bool success) {
  // If we can't connect, quit the loop...
  if (!success) {
    LOG(ERROR) << "Xdg-Desktop-Portal: Could not connect to signal";
    run_loop_->Quit();
  }
}

bool SecretPortalDBus::RetrieveSecret(std::string* output) {
  return RetrieveSecret({}, output);
}
