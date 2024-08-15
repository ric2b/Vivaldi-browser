---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: dbus-best-practices
title: ChromeOS D-Bus Best Practices
---

ChromeOS uses [D-Bus] for inter-process communication. At a high level, D-Bus
consists of a **system bus** that is managed by a `dbus-daemon` process.
**Clients** (typically either system daemons or Chrome) connect to the system
bus via `/run/dbus/system_bus_socket` and use it to communicate with each
other.

This document describes best practices for using D-Bus within ChromeOS system
daemons. For more information about using D-Bus within Chrome, see the [D-Bus
Usage in Chrome] document.

[TOC]

## Use Chrome's D-Bus bindings.

Since 2013, Chrome's C++ D-Bus bindings have been available on ChromeOS as part
of the [libchrome package] (specifically, in a `libbase-dbus` shared library).
These bindings integrate tightly with Chrome's message loop and callback classes
and follow our style guide and C++ best practices. They have been written to
avoid common pitfalls frequently encountered in older ChromeOS code (e.g.
mixing up service names and paths, failing to reply to method calls, etc.).
**All new C++ code should use Chrome's bindings.**

[libbrillo] (formerly known as libchromeos) provides additional code built on
top of libchrome's bindings, along with [DBusDaemon and DBusServiceDaemon
classes] that can reduce repetitive setup code. It may be useful in daemons that
already use other code from libbrillo. [chromeos-dbus-bindings] (also known as
`dbus-bindings-generator`) can be used to generate custom bindings for
libbrillo-based D-Bus daemons, as well.

Other D-Bus bindings are used by some older ChromeOS code:

*   [libdbus], the low-level D-Bus C API: Chrome's bindings are built on top of
    this library, but you should not use it directly. It requires careful memory
    management, doesn't integrate well with C++, and makes it easy to make
    mistakes. As the library's own documentation says, "if you use this
    low-level API directly, you're signing up for some pain."
*   [dbus-glib]: GLib implements its own [own object system] with subtle
    ref/unref semantics and expects to use its [own message loop]. Code using it
    is prone to memory leaks and use-after-free bugs, and it doesn't integrate
    easily with Chrome's message loop.
*   [dbus-c++]: This library is unmaintained. It makes heavy use of exceptions
    (which are forbidden in Chrome C++ code) and doesn't integrate well with the
    rest of our code.

Avoid using these bindings for new code and consider updating existing code to
use Chrome's bindings as well (as has been done for `session_manager`, `shill`,
`power_manager`, etc.).

## Get familiar with core D-Bus concepts.

D-Bus communication consists of **messages**, which are typically either
**method calls** or **signals**. A method call is a request from one process to
another that generates a reply (sometimes also called a response) or an error. A
signal is an asynchronous broadcast from one process that may be received by
multiple processes. Both may have associated arguments, which may be written or
read using the `dbus::MessageWriter` or `dbus::MessageReader` classes.

When a process connects to `dbus-daemon`, it is assigned a **unique connection
name**. Unique connection names are analogous to IP addresses. This name will
start with a colon, e.g. `:1.25`. Connections are established using `dbus::Bus`.

After connecting to D-Bus, a process can additionally request a **well-known
name** (also sometimes referred to as a **service name** in the ChromeOS
codebase). Well-known names are analogous to hostnames and allow processes to
find each other. By convention, these take the form `org.chromium.ProgramName`.
Ownership of a well-known name is requested using
`dbus::Bus::RequestOwnership()` or `dbus::Bus::RequestOwnershipAndBlock()`.

To receive method calls, a program typically registers an **object path** (also
sometimes referred to as a **service path** in the ChromeOS codebase). Object
paths take the form `/org/chromium/ProgramName`. A D-Bus object owned by the
current process is represented by `dbus::ExportedObject` and is registered using
`dbus::Bus::GetExportedObject()`. To get an **object proxy** that can be used to
call methods on or register for signals from an object exported by a remote
service, pass a service name and path to `dbus::Bus::GetObjectProxy()`.

A named group of methods and signals is called an **interface**. Interface names
look similar to well-known names, e.g. `org.chromium.ProgramNameInterface`.

## Use method calls for requests. Use signals for announcements.

Quoting the [upstream D-Bus tutorial],

> **Methods** are operations that can be invoked on an object, with optional
> input (aka arguments or "in parameters") and output (aka return values or "out
> parameters"). **Signals** are broadcasts from the object to any interested
> observers of the object; signals may contain a data payload.

Use a **method call** (see `dbus::MethodCall`,
`dbus::ExportedObject::ExportMethod()`, and `dbus::ObjectProxy::CallMethod*()`)
to send a request to another D-Bus client. Method calls trigger replies: errors
are sent to report failures, and (possibly-empty) replies are returned on
success.

Examples of requests made via method calls:

*   Instructing the power manager to shut down the system.
*   Instructing Chrome to lock the screen.
*   Asking the session manager whether a user is currently logged in.

Use a **signal** (see `dbus::Signal`, `dbus::ExportedObject::SendSignal()`, and
`dbus::ObjectProxy::ConnectToSignal()`) to announce a change in state that one
or more other D-Bus clients may be interested in.

Examples of events announced via signals:

*   A user has logged in or out.
*   The system has switched from AC to battery power.
*   The system has just resumed from suspend.

## Avoid depending heavily on D-Bus-specific concepts.

D-Bus provides additional functionality beyond the core concepts described
above. A process can export multiple objects, set properties on objects, and
implement the [object manager interface] to automatically inform other clients
about changes.

For example, consider a hypothetical `PeripheralManager` service that allows
configuration of external peripherals. One possible implementation would export
a new D-Bus object for each peripheral when it is first connected (e.g.
`/org/chromium/PeripheralManager/Mouse`,
`/org/chromium/PeripheralManager/Keyboard`, etc.) and allow other clients to
configure each peripheral by setting properties on its object.

Avoid using patterns like this in new code if possible:

*   Use of multiple objects can result in complicated client code. In the above
    example, a caller would now need to interact with multiple
    `dbus::ObjectProxy` objects.
*   D-Bus's property implementation relies heavily on variant types, again
    complicating client code.
*   There is a long-term goal to switch ChromeOS IPC from D-Bus to Mojo. Heavy
    use of D-Bus-specific concepts will make this task substantially harder.

Sticking to simpler D-Bus patterns, e.g. registering a single service name for
your process and exporting a single object, often results in code that is easier
to understand. As an alternate implementation for the above hypothetical
example, `PeripheralManager` could instead export a single
`/org/chromium/PeripheralManager` object. When a peripheral is connected,
`PeripheralManager` assigns a unique identifier to it (e.g. `Keyboard`, `Mouse`,
etc.) and announces the event via a `PeripheralConnected` signal. Other clients
can configure peripherals by passing previously-announced identifiers to a
`ConfigurePeripheral` method, perhaps also including protocol buffers describing
new configurations in an easy-to-read manner.

## Handle services starting slowly or restarting.

Processes often need to make D-Bus method calls to long-running services at
startup to register themselves or load or set initial state. Even when
dependencies are specified correctly in Upstart, the remote services may not
have connected to D-Bus or exported their methods by the time that the local
process attempts to call them. This results in log spam and has the potential to
actually delay startup if the race is handled by adding periodic retries (as the
client may wait longer than is necessary before retrying). Furthermore, services
sometimes crash and are restarted, and it's often necessary for the initial
D-Bus communication to be repeated after this happens to keep the system in a
fully-functional state. Chrome's D-Bus bindings make it straightforward to
handle both of these situations.

When your process starts, instead of calling a remote service immediately, pass
a callback to the remote service's
`dbus::ObjectProxy::WaitForServiceToBeAvailable()` method. Your callback will be
invoked asynchronously, either immediately if the service is already available
or when it becomes available in the future.

If you also need to know when the remote service restarts, the
`dbus::ObjectProxy::SetNameOwnerChangedCallback()` method can be used to learn
about a remote service starting or stopping. The callback passed to this method
will be run with a non-empty `new_owner` argument when the service starts and
with an empty `new_owner` argument when it stops. For libbrillo users,
`brillo::dbus_utils::DBusServiceWatcher` may be handy.

Even more generally, if you need to know about all D-Bus name ownership changes,
you can watch for `org.freedesktop.DBus.NameOwnerChanged` signals emitted by
`dbus-daemon`. For example, powerd uses this technique to watch for anonymous
clients that previously registered suspend delays exiting.
`dbus::Bus::ListenForServiceOwnerChange()` supports more-targeted listening for
change in ownership of a single name.

## Use `system_api` to share constants

The [system_api] repository is available within Chrome and all other ChromeOS
repositories. All D-Bus-related constants or other definitions that need to be
shared between repositories should live in `system_api`:

*   service names and paths
*   interface, method, and signal names
*   enum values used as arguments in method calls or signals
*   definitions of protocol buffers encoded as arguments

`system_api` is manually uprev-ed within Chrome by updating the SHA1 within the
top-level `DEPS` file.

## Methods’ names should contain verbs. Signals’ names should describe what just happened.

Like C++ methods, D-Bus methods should have names containing imperative verbs
describing the action that will be performed:

*   `StoreDeviceLocalAccountPolicy` is exported by the session manager and
    called by Chrome to tell the session manager to store policy information.
*   `HandleLockScreenDismissed` is exported by the session manager and called by
    Chrome once the user authenticates successfully and the lock screen has been
    dismissed.
*   `RegisterSuspendDelay` is exported by the power manager and called by other
    D-Bus clients to indicate that they want to be notified when the system is
    about to suspend. The client passes a timeout and receives a response
    containing a unique ID.
*   `HandleSuspendReadiness` is exported by the power manager and called by
    D-Bus clients that have previously registered a suspend delay once they’re
    ready for the system to be suspended.

D-Bus signal names should describe the event that just occurred:

*   `KeyboardBrightnessChanged` is emitted by the power manager whenever the
    keyboard backlight brightness level changes.
*   `SuspendStateChanged` is emitted by the power manager when the system
    suspends or resumes.
*   `SuspendImminent` is emitted by the power manager when the system is about
    to suspend; clients that have previously registered a suspend delay pass an
    ID from this signal to HandleSuspendReadiness once they’re ready to suspend.

Note that several of the above method calls are prefixed with the word `Handle`
to indicate that the D-Bus client exporting the method is expected to handle the
occurrence of an outside event.  It’s sometimes possible to instead use signals
for situations like these: instead of Chrome calling the session manager’s
`HandleLockScreenDismissed` method, the session manager could watch for a
`LockScreenDismissed` signal emitted by Chrome.  In the case of
`HandleSuspendReadiness`, the power manager would need to listen for
`SuspendReady` signals from an arbitrary number of other clients, so it’s
simpler to have those clients notify the power manager directly when they’re
ready by calling this method.

## Make method calls asynchronous whenever practical.

The `dbus::ObjectProxy` class provides both `CallMethod...` and
`CallMethodAndBlock...` groups of methods for making method calls. The former
methods return immediately and run a callback asynchronously after a reply is
received, while the latter methods block until a reply is received or a timeout
(by default, thirty seconds!) is hit. If you use the synchronous
`CallMethodAndBlock...` methods to call a remote service that is hanging, your
service will also hang. This is unacceptable for most production code: even if
your service isn't user-facing, it will probably eventually be called (either
directly or indirectly) by something that is.

## Be careful when making changes to in-use methods and signals.

D-Bus relies on a message's sender and receiver agreeing about the format of the
message's contents. A message contains a sequence of arguments of various types,
and it's possible to change an in-use method or signal's arguments by ensuring
that the receiver is able to read both the old and new format (e.g. by
inspecting the return values of `dbus::MessageReader::Pop...` to determine which
arguments are present or missing).

If you're using `chromeos-dbus-bindings`, you can add an annotation to a
`<method>` element requesting that the generated handler method receive a
`dbus::MethodCall*` instead of individual arguments, allowing you to handle
missing arguments using `dbus::MessageReader`:

```xml
<annotation name="org.chromium.DBus.Method.Kind" value="raw"/>
```

The generated method's signature will look like this:

```c++
void MethodName(dbus::MethodCall* method_call,
                brillo::dbus_utils::ResponseSender sender);
```

Handling multiple signatures for a method results in fragile, complicated code,
and should only be used temporarily during the transition to a new message
signature.

If you need to make more dramatic changes to a method's signature and are unable
to use the above approach, you can:

*   Add a temporary method with a new name and the new signature to the server.
*   Update all callers to call the new, temporary method with updated arguments.
*   Update the server so the old method also has the new signature.
*   Update all callers to call the old method again.
*   Remove the new, temporary method from the server.

Signals can be updated in a similar fashion by making the server emit both old
and new versions with different names.

Note that you may need to wait a week or more between each change if the code
spans the Chrome repository and one or more ChromeOS repositories, since it
takes time for updated versions of Chrome to be integrated into the OS and since
developers also sometimes deploy tip-of-tree versions of Chrome to older OS
images. As a result, make the changes in-place as described earlier if possible.

If you find yourself needing to make frequent changes to messages or want to
introduce optional arguments, protocol buffers are a better choice. See the next
item.

## Consider using protocol buffers for complex messages.

[Protocol buffers] provide an extensible way to serialize arbitrary data. Fields
in a protocol buffer are referenced via human-readable names in code but are
also assigned fixed integer identifiers; messages sent by newer clients remain
readable by older clients even after fields have been added or removed. This is
particularly useful for maintaining compatibility between different versions of
Chrome and the rest of ChromeOS.

Arguments of various types can be written to D-Bus messages via
`dbus::MessageWriter` and read using `dbus::MessageReader`, but this approach
has drawbacks:

*   Code that pops multiple arguments from a message one at a time is verbose
    and fragile.
*   Code that uses D-Bus string-to-variant dictionaries is even more verbose and
    fragile.
*   There is no compile-time checking that the correct argument types are
    written or read from messages.
*   Making changes to an existing message’s arguments is time-consuming, since
    the reader must first be updated to handle messages with both the old and
    new argument signature.

For methods and signals with non-trivial argument signatures, or with signatures
that are likely to be extended in the future, using protocol buffers with fields
that have been marked `optional` (as opposed to `required`) makes it much easier
to change messages later.

As a hypothetical example, for a method named `MyMethod`, consider defining
`MyMethodRequest` and `MyMethodResponse` (or `MyMethodReply`) protocol buffers
and passing them (after serialization) as single input and output arguments:

```proto
// Passed to MyMethod.
message MyMethodRequest {
  optional int32 query_id = 1;
  optional string query_text = 2;
}

// Returned by MyMethod.
message MyMethodResponse {
  optional string matched_text = 1;
}
```

It's advisable to define dedicated request and response protocol buffers for the
method even if it already operates on existing protocol buffers. For example, if
the hypothetical `MyMethod` operates on a supplied `Foo` protocol buffer and
produces a `Bar` protocol buffer, wrap the existing messages:

```proto
message MyMethodRequest {
  optional Foo foo = 1;
}

message MyMethodResponse {
  optional Bar bar = 1;
}
```

This approach makes it possible to later add additional parameters while
maintaining backward and forward compatibility, and without modifying the
definitions of `Foo` and `Bar` (which may also be used elsewhere):

```proto
message MyMethodRequest {
  optional Foo foo = 1;
  optional int32 priority = 2;
  optional string request_key = 3;
}
```

Chrome's D-Bus bindings support serializing and deserializing generic protocol
buffers: see `dbus::MessageWriter::AppendProtoAsArrayOfBytes()` and
`dbus::MessageReader::PopArrayOfBytesAsProto()`.

The only downside of using protocol buffers over D-Bus is that they make it
more challenging to visually inspect signal arguments using `dbus-monitor`
(since the encoded message will be displayed as a byte array).

## Always send a reply or error after receiving a method call.

D-Bus limits the number of in-flight method calls between two clients. If a
client receives a method call but does not send a reply, the call will stay open
indefinitely. At some point, additional method calls will not be delivered. In
the past, this subtlety has caused at least one [difficult-to-track-down bug].
If your process exports method calls, make sure you always send replies or
errors by running the `dbus::ExportedObject::ResponseSender` passed to your
method callback.

## Use D-Bus error replies when appropriate

D-Bus contains the concept of an _error reply_. This is a subtype of a normal
reply that is used to report errors. Chrome's bindings implement error replies
via the `dbus::ErrorResponse` class.

When reporting a simple failure in response to a method call, it's often best to
send an error reply instead of sending a normal reply containing a boolean
`success` argument or a protocol buffer with a `success` field. If errors are
reported within normal replies, callers need to implement error handling at
multiple levels:

*   When a normal reply is received, the `success` arg needs to be inspected.
*   Error replies will still be received if the method call failed at the bus
    level, e.g. due to the receiver being unresponsive or not exporting the
    called method.

More-structured error reporting may still be needed in some cases, e.g. when
error codes are returned, but note that D-Bus error replies also contain names
that can be used to distinguish between different types of errors.

## Configure permissions correctly.

XML files in `/etc/dbus-1` are used to configure the D-Bus security policy. The
toplevel `/etc/dbus-1/system.conf` file disallows all name ownership requests
and method calls by default on the system bus. Per-service files in the
`/etc/dbus-1/system.d` directory (or `/opt/google/chrome/dbus`, in the case of
services exported by Chrome) are used to add exceptions. Since the default
policy denies name requests and method calls, `<allow>` directives (rather than
`<deny>`) should appear in these per-service files.

It is advisable to use both `send_destination` and `send_interface` in `<allow>`
directives that grant permission to invoke method calls on a given service.
Unintuitively, `send_destination="org.example.Service"` matches all calls to
*any* service exported by the client that owns the `org.example.Service` name,
so additionally specifying `send_interface="org.example.ServiceInterface"`
ensures that only those handled by the intended service will be permitted.

## Always delegate service startup to Upstart.

D-Bus has the ability to [start a service] when a message is sent to its
well-known name. This functionality is controlled by configuration files in
`/usr/share/dbus-1/system-services`. ChromeOS generally uses [Upstart] to
manage services, and D-Bus service activation is much less flexible (e.g. no
support for dependencies). D-Bus activation is currently used for short-lived
processes that need to run with a UID different from the process that starts
them. Start your services via Upstart unless there are compelling reasons to use
D-Bus activation.

If D-Bus activation still seems like the best tool (e.g. you're writing a
[sandboxed] daemon for a rarely-used feature), use Upstart to manage
your daemon and use D-Bus activation to start the Upstart job when it's
needed. This approach allows Upstart to manage the lifecycle of the service,
for example, terminate your daemon when the user logs out (via `stop on stopping
ui` in its Upstart config file). Another example is during shutdown: an active
service can prevent the filesystem(s) from getting unmounted cleanly and
delegating the lifecycle of the service to Upstart helps prevent such
situations.

For example, for a service named `org.chromium.MyService`, create a
`/usr/share/dbus-1/system-services/org.chromium.MyService.service` file:

```ini
[D-BUS Service]
Name=org.chromium.MyService
Exec=/sbin/start myservice
User=root
```

Then add an `/etc/init/myservice.conf` file that defines your Upstart job
without including a `start on` clause:

```sh
...

# This is started by D-Bus service activation through
# org.chromium.MyService.service.
stop on stopping ui
respawn

...

post-start exec minijail0 -u myservice -g myservice /usr/bin/gdbus \
    wait --system --timeout 15 org.chromium.MyService
```

The `post-start` line ensures that `start myservice` will wait for the D-Bus
service to be available before returning.

This technique is used in [this smbproviderd change].

## Know how to dig deeper.

Running `dbus-monitor --system` as the `root` user dumps live D-Bus traffic. By
default, only signals are included. To let `dbus-monitor` also see method calls,
create a file named `/etc/dbus-1/system.d/eavesdrop.conf` with the following
contents and reboot the system:

```xml
<!DOCTYPE busconfig PUBLIC
 "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <policy user="root">
    <allow eavesdrop="true"/>
  </policy>
</busconfig>
```

The `dbus-send` program sends D-Bus messages. For example, the following command
makes a method call to the `powerd` process asking it to suspend the system:

```bash
dbus-send --system --print-reply --type=method_call \
  --dest=org.chromium.PowerManager \
  /org/chromium/PowerManager \
  org.chromium.PowerManager.RequestSuspend
```

[D-Bus]: https://www.freedesktop.org/wiki/Software/dbus/
[D-Bus Usage in Chrome]: /chromium-os/developer-library/guides/ipc/dbus-in-chrome/
[libchrome package]: https://android.googlesource.com/platform/external/libchrome/+/HEAD/dbus/
[libbrillo]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/libbrillo/brillo/dbus/
[DBusDaemon and DBusServiceDaemon classes]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/libbrillo/brillo/daemons/dbus_daemon.h
[chromeos-dbus-bindings]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/chromeos-dbus-bindings/
[libdbus]: https://dbus.freedesktop.org/doc/api/html/index.html
[dbus-glib]: https://dbus.freedesktop.org/doc/dbus-glib/
[own object system]: https://developer.gnome.org/gobject/stable/gobject-The-Base-Object-Type.html
[own message loop]: https://developer.gnome.org/glib/stable/glib-The-Main-Event-Loop.html
[dbus-c++]: http://dbus-cplusplus.sourceforge.net/
[upstream D-Bus tutorial]: http://dbus.freedesktop.org/doc/dbus-tutorial.html#members
[object manager interface]: https://dbus.freedesktop.org/doc/dbus-specification.html#standard-interfaces-objectmanager
[system_api]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/system_api
[Protocol buffers]: https://github.com/google/protobuf
[difficult-to-track-down bug]: https://crbug.com/208247
[start a service]: https://dbus.freedesktop.org/doc/dbus-specification.html#message-bus-starting-services
[Upstart]: http://upstart.ubuntu.com/
[sandboxed]: /chromium-os/developer-library/guides/development/sandboxing/
[this smbproviderd change]: https://crrev.com/c/982404
