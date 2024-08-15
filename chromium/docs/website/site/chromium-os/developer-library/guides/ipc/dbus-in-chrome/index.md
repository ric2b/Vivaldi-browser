---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: dbus-in-chrome
title: ChromeOS D-Bus Usage in Chrome
---

[D-Bus] is used to perform interprocess communication on ChromeOS. This
document describes how to use D-Bus for communication between Chrome and system
daemons.

> See the [D-Bus Best Practices] document for high-level advice on using D-Bus.
> While that document focuses on how to use D-Bus within system daemons, much of
> it is relevant to Chrome as well.

[TOC]

## Sharing constants

The [system_api] repository contains C++ constants and protocol buffer `.proto`
files that are shared between Chrome and ChromeOS system daemons. This includes
D-Bus service names, paths, and interfaces, signal and method names, and enum
values that are passed as D-Bus arguments.

System daemons essentially always use the latest version of the repository,
while Chrome uses the revision specified in [src/DEPS]. Exercise care when
making use of new constants or removing deprecated constants. To create a change
that updates the version used by Chrome to ToT, run `roll-dep
src/third_party/cros_system_api`.

## Creating Chrome D-Bus services

Receiving method calls or emitting signals requires registering a service.
Chrome registers services using a variety of service names, including
`org.chromium.DisplayService` and `org.chromium.NetworkProxyService`. Services
implement the [ash::CrosDBusService::ServiceProviderInterface] interface.

### Code location

The [mus+ash] project is separating Chrome's shell/window-management code (i.e.
`//ash`) from its browser code (i.e. `//chrome`). If a service does not depend
on any code under `//chrome`, its service provider class should live in
[ash/dbus] and be instantiated by [ash_dbus_services.cc].

Services with implementations that depend on `//chrome` should be implemented
within [chrome/browser/ash/dbus] and instantiated by
[chrome_browser_main_parts_ash.cc].

Services provided by Lacros should be in [chromeos/lacros/dbus].

### Policy files

In order for Chrome to be able to take ownership of a service name and for other
processes to be able to call its methods, each service also needs a `.conf` XML
policy file in [ash/dbus] or [chrome/browser/ash/dbus]. Policy files are
loaded by `dbus-daemon` (which implements the system bus) and specify which Unix
users can own service names or call services' methods. Chrome's policy files are
installed to `/opt/google/chrome/dbus` and must each be listed in the
`dbus_service_files` target in [ash/BUILD.gn] or
[chrome/browser/chromeos/BUILD.gn].

See [D-Bus Best Practices] for more information about D-Bus permissions.

## Using system daemons' D-Bus services

To call methods exported by system daemons or observe signals, Chrome uses
`Client` classes located under [chromeos/dbus]. Ash specific clients are located
under [chromeos/ash/components/dbus].

D-Bus clients for lacros is not recommended without a specific reason because
lacros has version skews. You should consider to use crosapi so that ash proxies
the D-Bus request.

**Chrome's D-Bus-related code is not thread-safe and runs on the UI thread in
the browser process.**

> System daemons must install their own D-Bus policy files granting
> `send_destination` privileges to the `chronos` user in order to receive method
> calls from Chrome. Search for "permissions" in [D-Bus Best Practices] for
> details.

`Client` classes are owned by `chromeos::DBusClientCommon` (instantiated in all
processes) and `chromeos::DBusClientsBrowser` (instantiated only in the browser
process), which are both owned by `chromeos::DBusThreadManager`. `Client`
classes are currently accessed via the `DBusThreadManager` singleton.

When adding a new `Client` class, please follow the patterns used by the
existing code:

### Define real and fake implementations

For a service named "foo", define a `FooClient` interface in `foo_client.h`. In
`foo_client.cc`, declare and define a `FooClientImpl` class that implements the
interface. Add `fake_foo_client.h` and `fake_foo_client.cc` files defining a
`FakeFooClient` implementation that can be used in unit tests and when running a
ChromeOS build of Chrome on a workstation where the system daemons aren't
present.

### Keep your `ClientImpl` class minimal

Ideally, client classes should only connect to signals, call methods, and
serialize and deserialize D-Bus message arguments. The real implementations of
client interfaces aren't exercised by unit tests, so keep your actual logic in
the class that uses the client interface, where it can be tested using the fake
client implementation.

More concretely, a `Client` interface should expose public methods with the same
names as the corresponding D-Bus methods and optionally define a nested
`Observer` interface that can be used to receive notifications about signals.
`FakeClientImpl` can additionally expose setters that specify canned values to
be returned by methods and `NotifyObserversAboutSomeSignal` methods that call a
method on all observers to simulate the receipt of a signal.

If a D-Bus method takes a serialized protocol buffer as an argument, the client
class's corresponding method should take a const reference to that protobuf as
its argument (rather than individual args corresponding to the protobuf's
fields). Observer interfaces and method callbacks should also take protobuf args
when possible. Search for "complex messages" in [D-Bus Best Practices] for
additional information.

### Method calls must be asynchronous

Since a `Client` class's code runs on Chrome's UI thread, D-Bus method calls to
system daemons (which may be otherwise occupied or even hanging) must be
asynchronous. This may be accomplished with code similar to the following:

```c++
// foo_client.h:

#include "chromeos/dbus/method_call_status.h"

class FooClient {
 public:
  ...
  // Calls the daemon's GetSomeValue D-Bus method and asynchronously
  // invokes |callback| with the result.
  virtual void GetSomeValue(DBusMethodCallback<double> callback) = 0;
  ...
};

// foo_client.cc:

namespace {

// Handles responses to GetSomeValue method calls.
void OnGetSomeValue(DBusMethodCallback<double> callback,
                    dbus::Response* response) {
  if (!response) {
    // CallMethod() will already log an error if the method call fails.
    std::move(callback).Run(base::nullopt);
    return;
  }

  double value = 0.0;
  dbus::MessageReader reader(response);
  if (!reader.PopDouble(&value)) {
    LOG(ERROR) << "Error reading response: " << response->ToString();
    std::move(callback).Run(base::nullopt);
    return;
  }
  std::move(callback).Run(value);
}

}  // namespace

...

void FooClientImpl::GetSomeValue(DBusMethodCallback<int> callback) override {
  dbus::MethodCall method_call(foo::kFooInterface, foo::kGetSomeValueMethod);
  foo_proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                         base::BindOnce(&OnGetSomeValue, std::move(callback)));
}
```

When writing fake implementations of methods that receive and run callbacks,
post the callback to the message loop instead of running it synchronously. This
matches the calling pattern used in the real implementation, where replies from
system daemons are received asynchronously.

### Don't call services before they're available

Chrome is started in parallel with many system daemons, and your code may run
before the daemon it wants to communicate with has exported its methods or taken
ownership of its service name. If you immediately try to make a method call to
the daemon, it's likely to fail and log annoying, unactionable errors to
Chrome's log file. To avoid this unsightly gaffe, add a
`WaitForServiceToBeAvailable` method to the `Client` interface that callers can
use to defer their method calls until the daemon is ready. See some of the
existing `Client` classes, e.g. `chromeos::CryptohomeClient` or
`chromeos::DebugDaemonClient, for examples of this.

## Sharing state between processes

If you have state shared between Chrome and a system daemon, you'll need to
think about how you'll get both processes back into sync when one of them
restarts or when one of them starts before the other is ready.

### System daemon restarts

While it's usually not expected, the system daemon that you're communicating
with may restart while the system is running. This can happen if the daemon
crashes, of course, but it can also happen when a developer pushes a new version
of the daemon to their development device. To handle this case, you can call
`dbus::ObjectProxy::SetNameOwnerChangedCallback` in your `ClientImpl` class and
notify observers when you see a non-empty "new owner" for the daemon's service
name. See `chromeos::PowerManagerClient` for an example of this.

### Chrome restarts

Chrome restarts are more common. Crashes happen, but Chrome is also
intentionally restarted when a user logs out and sometimes even when a user logs
in (to pick up changes to `chrome://flags`). System daemons can use the same
`SetNameOwnerChangedCallback` technique described above to learn about Chrome
restarts (using the relevant Chrome D-Bus service name).

If Chrome tracks your daemon's state by observing its D-Bus signals, you may
need to provide a method that Chrome can call when it starts or restarts to get
the correct initial state from the daemon.

[D-Bus]: https://www.freedesktop.org/wiki/Software/dbus/
[D-Bus Best Practices]: /chromium-os/developer-library/guides/ipc/dbus-best-practices/
[system_api]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/system_api
[src/DEPS]: https://chromium.googlesource.com/chromium/src/+/HEAD/DEPS
[mus+ash]: https://www.chromium.org/developers/mus-ash
[ash::CrosDBusService::ServiceProviderInterface]: https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/services/cros_dbus_service.h
[ash/dbus]: https://chromium.googlesource.com/chromium/src/+/HEAD/ash/dbus/
[ash_dbus_services.cc]: https://chromium.googlesource.com/chromium/src/+/HEAD/ash/dbus/ash_dbus_services.cc
[chrome/browser/ash/dbus]: https://chromium.googlesource.com/chromium/src/+/HEAD/chrome/browser/ash/dbus/
[chrome_browser_main_parts_ash.cc]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/chrome_browser_main_parts_ash.cc
[chromeos/lacros/dbus]: https://source.chromium.org/chromium/chromium/src/+/main:chromeos/lacros/dbus/
[ash/BUILD.gn]: https://chromium.googlesource.com/chromium/src/+/HEAD/ash/BUILD.gn
[chrome/browser/chromeos/BUILD.gn]: https://chromium.googlesource.com/chromium/src/+/HEAD/chrome/browser/chromeos/BUILD.gn
[chromeos/dbus]: https://chromium.googlesource.com/chromium/src/+/HEAD/chromeos/dbus
