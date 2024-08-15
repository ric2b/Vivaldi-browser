---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: unit-tests
title: Best practices for writing ChromeOS unit tests
---

[Unit tests] verify the correctness of a small piece of source code (e.g. a
class or function) and live alongside the code they're testing. When a Portage
package with `src_test` defined in its ebuild file is compiled with a command
like `FEATURES=test emerge-<board> <package>`, the package's unit tests are
compiled into one or more executables, which are executed immediately by the
system performing the build. If any unit tests fail, the entire build fails.

C++ unit tests are written using [Google Test] and typically live in files with
`_unittest.cc` or `_test.cc` suffixes (e.g. the unit tests for a source file
named `foo/bar.cc` should live in `foo/bar_unittest.cc`).

The [unit testing document] has more details about how unit tests are executed.

[TOC]

## Prefer unit tests to functional tests

[Functional tests] (sometimes also called *integration tests*) are written in Go
using the [Tast] framework or in Python using the [Autotest] framework. They run
on a live ChromeOS system and verify the end-to-end functionality of multiple
components. As a result, they do not run automatically while a build is being
performed, and take substantially longer to complete than unit tests.

Functional tests are needed to verify that different components work together.
For example, a test that verifies that the user is able to log into Chrome
indirectly checks that:

-   The kernel boots successfully.
-   Upstart runs `session_manager`.
-   `session_manager` runs Chrome.
-   Chrome's login code works.
-   `session_manager` and Chrome are able to communicate over D-Bus.
-   `cryptohome` mounts user's encrypted home directories.
-   [many other things]

It's important to test all of these things, but we don't want to use functional
tests for all of our testing:

-   Developers can't run functional tests without booting a ChromeOS device (or
    virtual machine) and deploying their code to it.
-   A no-op Autotest-based test takes more than 20 seconds to run, compared to a
    tiny fraction of a second for a no-op unit test. Tast tests have minimal
    overhead, but they're still slower than unit tests.
-   There are many things that can go wrong when booting a full ChromeOS system
    (e.g. hardware issues, network issues, legitimate but rare races in Chrome
    OS), and functional tests are much more likely to experience flakiness than
    unit tests.
-   Because functional tests are so slow, the ChromeOS [commit queue] and
    pre-CQ aren't able to run many of them (whereas all unit tests are built and
    run). If a component is only exercised by functional tests, it's much less
    likely that newly-introduced bugs will be caught before they're checked in.
-   It's difficult to know which functional tests to run to verify a given code
    change. There are currently nearly a thousand Autotest-based tests checked
    in; running them all is infeasible (and many have been abandoned and don't
    even pass anymore). In contrast, a given package's unit tests are easy to
    run and are always expected to pass.
-   Even when functional tests appear to provide complete coverage of a system,
    they often exercise only a single best-case path through the code. Due to
    their slowness, it's often impossible to cover all the paths that can be
    taken in production.

**When you can get similar coverage using either type of test (i.e. the test
exercises a single component), always use unit tests.** This requires some
planning when writing the code; the rest of this document gives advice for doing
that.

## Declaring unit test dependencies

Sometimes your package will need additional host tools or build dependencies to
run unit tests. The recommended way is to use the `test` USE flag. This flag
will be set when building your package with `FEATURES=test`.

```
IUSE="test"
BDEPEND="test? ( sys-apps/foo )"
```

A few things to note when using the `test` USE flag:

1.  It should be assumed that the `test` USE flag will only be enabled for your
    package when running with `FEATURES=test`. That is, your dependencies aren't
    guaranteed to have been built with their `test` USE flag enabled.
1.  You should never place a `test` USE constraint on a dependency (e.g.,
    `DEPEND="sys-apps/foo[test?]` or `DEPEND="test? ( sys-apps/foo[test] )`).
    Supporting this would require transitive rebuilds and violates the first
    point.
1.  Your package's public ABI should stay stable regardless of the `test` USE
    flag. This is because there is no guarantee that a dependency has been
    compiled with their `test` USE flag enabled.

See the Gentoo documentation on [test dependencies] for more information.

[test dependencies]: https://devmanual.gentoo.org/general-concepts/dependencies/index.html#test-dependencies

### Exclude unit test files from the OS image

Since we want the package to have a stable public ABI regardless of the `test`
USE flag, we want to always compile any unit test helper libraries. These
libraries should be installed into `/build`. This folder is excluded by default
when creating an OS image.

```
insinto /build/[bin|lib|...]
doins "${file}"
```

## Use well-defined public interfaces

Unit tests verify that functions or classes produce correct output when supplied
various inputs; "output" may refer to functions' or methods' return values,
changes made to the system (e.g. files that are written), or actions that are
performed by making calls to other classes. **Test the code's output instead of
its internal state (i.e. how it produces the output).**

Instead of using the `friend` keyword and `FRIEND_TEST` macro to let tests
inspect and modify a class's private data members and call its private methods,
make unit tests exercise the class's public interface: test the API rather than
the implementation. This helps avoid tests that simply mirror the implementation
line-for-line.

If tests have a true need to exercise private implementation details (e.g. to
check whether a private timer is running and manually trigger it), add dedicated
public methods with `ForTest` suffixes (or `for_test`, in the case of inlined
methods) that can be called by tests. If tests need even more control over a
class's internals, consider defining a dedicated `TestApi` internal class:

```c++
// delayed_sound_player.h:

// Plays a sound after a while.
class DelayedSoundPlayer {
 public:
  // Helps tests interact with DelayedSoundPlayer.
  class TestApi {
   public:
    explicit TestApi(DelayedSoundPlayer* player);
    ~TestApi() = default;

    // Triggers and stops |player_|'s timer and returns true.
    // Does nothing and returns false if the timer wasn't running.
    bool TriggerTimer() WARN_UNUSED_RESULT;
    ...

   private:
    DelayedSoundPlayer* player_;  // Not owned.

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  ...

 private:
  // Used to play a sound after a delay.
  base::OneShotTimer timer_;
  ...
};

// delayed_sound_player.cc:

DelayedSoundPlayer::TestApi::TestApi(DelayedSoundPlayer* player)
    : player_(player) {}

bool DelayedSoundPlayer::TestApi::TriggerTimer() {
  if (!player_->timer_.IsRunning())
    return false;

  // Copy the task to a local variable so we can stop the timer first (in case
  // the task wants to restart the timer).
  base::Closure task = player_->timer_->user_task();
  player_->timer_->Stop();
  task.Run();
  return true;
}

// delayed_sound_player_unittest.cc:

TEST_F(DelayedSoundPlayerTest, Timer) {
  DelayedSoundPlayer player;
  TestApi api(&player);
  ASSERT_TRUE(api.TriggerTimer());
  // Check that a sound was played.
}
```

This makes the points of interaction between the tests and the code being tested
obvious, and makes it less likely that test code will gain brittle dependencies
on the internal implementation that make refactorings difficult in the future.
Care must still be taken to ensure that the API limits the scope of
implementation details that are exposed to tests, however.

## Avoiding side effects

Sometimes the class that's being tested does work that needs to be avoided when
running as part of a test instead of on a real device -- imagine code that calls
`ioctl()` or reboots the system, for instance.

It can be tempting to add additional logic to the code that's being tested to
instruct it to avoid actually doing anything (e.g. a `running_on_real_system`
member), or to mark some methods as virtual and then override them within tests
using [Google Mock] (known as a "partial mock"), but both of these approaches
make the code harder to read ("Why is this method virtual? Oh, just for
testing") and make it easy to accidentally overlook dangerous "real" code and
execute it within tests.

**Instead, try to structure your code so it's easy for tests to inject their own
implementations of functionality that can only run on real systems.**

### Delegate interfaces

If the untestable portion of the code is trivial, it can be moved out of the
class and into a delegate interface, which can then be stubbed out in tests. For
example, imagine a class that's responsible for rebooting the system:

```c++
// power_manager.h:

class PowerManager {
 public:
  ...
  // Does a bunch of work to prepare for reboot and then reboots the system.
  // Returns false on failure.
  bool Reboot();
  ...
};
```

Instead of rebooting the system directly, the class can define a nested
`Delegate` interface and ask it to do the actual rebooting:

```c++
// power_manager.h:

class PowerManager {
 public:
  // The Delegate interface performs work on behalf of PowerManager.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Asks the system to reboot itself. Returns false on failure.
    virtual bool RequestReboot() = 0;
  };

  explicit PowerManager(std::unique_ptr<Delegate> delegate);
  ...

 private:
  std::unique_ptr<Delegate> delegate_;
  ...
};

// power_manager.cc:

PowerManager::PowerManager(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate) {}

bool PowerManager::Reboot() {
  // Do lots of stuff to prepare to reboot.
  ...
  return delegate_->RequestReboot();
}
```

In the shipping version of the code, pass a "real" implementation of the
delegate:

```c++
// main.cc:

namespace {

class RealDelegate : public PowerManager::Delegate {
 public:
  RealDelegate() = default;
  ~RealDelegate() override = default;

  // PowerManager::Delegate:
  bool RequestReboot() override {
    return system("/sbin/reboot") == 0;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RealDelegate);
};

}  // namespace

int main(int argc, char** argv) {
  ...
  PowerManager manager(std::make_unique<RealDelegate>());
  ...
}
```

Inside of the test file, define a stub implementation of `Delegate` that keeps
track of how many times `RequestReboot` has been called and returns a canned
result:

```c++
// power_manager_unittest.cc:

namespace {

// Stub implementation of PowerManager::Delegate for testing that just keeps
// track of what has been requested.
class StubDelegate : public PowerManager::Delegate {
 public:
  StubDelegate() = default;
  ~StubDelegate() override = default;

  int num_reboots() const { return num_reboots_; }
  void set_reboot_result(bool result) { reboot_result_ = result; }

  // PowerManager::Delegate:
  bool RequestReboot() override {
    num_reboots_++;
    return reboot_result_;
  }

 private:
  // Number of times RequestReboot has been called.
  int num_reboots_ = 0;

  // Result to return from RequestReboot.
  bool reboot_result_ = true;

  DISALLOW_COPY_AND_ASSIGN(StubDelegate);
};

}  // namespace
```

Now the test can inject a `StubDelegate` and use it to verify that
`RequestReboot` was called:

```c++
TEST_F(PowerManagerTest, Reboot) {
  // |delegate| is owned by |manager|.
  StubDelegate* delegate = new StubDelegate();
  PowerManager manager(base::WrapUnique(delegate));
  EXPECT_EQ(0, delegate->num_reboots());

  EXPECT_TRUE(manager.Reboot());
  EXPECT_EQ(1, delegate->num_reboots());

  // Check that PowerManager reports failure.
  delegate->set_reboot_result(false);
  EXPECT_FALSE(manager.Reboot());
  EXPECT_EQ(2, delegate->num_reboots());
}
```

## Kernel interfaces

Unit tests must be portable between kernel versions, configs, loaded modules,
etc... To that end, unit tests must not depend on how the build system's kernel
is setup in order to pass. Our builders upgrade versions on an independent cycle
from CrOS projects, often times in response to known exploits. It is usually not
acceptable or possible to rollback such updates.

While the exact functionality available to unit tests is not precisely defined
or enforced, avoid relying on uncommon interfaces (e.g. syscalls) or any non-
standard virtual driver or any hardware-specific functionality. If a test needs
to validate such interfaces, they'd be better written as integration tests that
run on VMs, especially so they can be executed on multiple kernel versions that
much the configs used on released CrOS devices.

Some high level guarantees unit tests can assume:

*   Linux 4.4

[Kernel interfaces]: #kernel-interfaces

## Filesystem interaction

If the code being tested reads or writes large amounts of data from files, it's
advisable to stub out those interactions in unit tests to improve performance.
Stubbing may also be necessary when exercising code that handles some classes of
errors that are difficult to trigger within tests (e.g. permission errors).

When the code's interaction with the filesystem is limited and fast, it's often
better to let it operate on the real filesystem, but in a controlled way that
doesn't have negative effects on the rest of the system where the tests are
being run.

### Inject temporary directories

If the code being tested needs to read or write multiple files within a
directory, try to structure it so the directory is passed into it:

```c++
// Reads and writes files within |dir|, a sysfs device directory.
bool UpdateDevice(const base::FilePath& dir) {
  ...
}
```

In the test, use `base::ScopedTempDir` to create a temporary directory that will
be automatically cleaned up when the test is done:

```c++
#include <base/files/scoped_temp_dir.h>
...

TEST_F(MyTest, UpdateDevice) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  // Create needed files under |temp_dir|.
  ...
  ASSERT_TRUE(UpdateDevice(temp_dir.GetPath()));
  // Verify that expected files were written under |temp_dir|.
  ...
}
```

### Make path constants overridable

If a class being tested uses hardcoded file paths, consider adding the
ability to override them from tests:

```c++
// backlight_lister.h:

class BacklightLister {
 public:
  BacklightLister();
  ...

  // Should be called immediately after the object is created.
  void set_base_dir_for_test(const base::FilePath& dir) {
    base_dir_ = path;
  }
  ...

 private:
  // Base sysfs directory under which backlight devices are located.
  base::FilePath base_dir_;
  ...
};

// backlight_lister.cc:

namespace {

// Real sysfs directory under which backlight devices are located.
const char kDefaultBaseDir[] = "/sys/class/backlight";

}  // namespace

BacklightLister::BacklightLister() : base_dir_(kDefaultBaseDir) {}
...
```

In the test, use `set_base_dir_for_test` to inject a temporary directory into
the class:

```c++
TEST_F(BacklightListerTest, ListBacklights) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  // Create fake device files under |temp_dir|.
  ...

  BacklightLister lister;
  lister.set_base_dir_for_test(temp_dir.GetPath());
  // Verify that |lister| lists expected devices.
  ...
}
```

### Filesystems

Per the [Kernel interfaces] guidelines, unit tests may not mount arbitrary
filesystems, especially non-standard ones. Access to [extended attributes
(xattrs)](https://man7.org/linux/man-pages/man7/xattr.7.html) and
SELinux labels/contexts are not guaranteed. Such tests should be a VM
integration test instead.

### Runtime paths

Common paths like `/run` and `/run/lock` are initialized fresh for each package.
Other paths, like `/var`, should be avoided.

### Build & source files

Often unit tests want supplemental files when running, and these are checked in
directly rather than generating them on demand. Access to common directories are
provided via environment variables.

*   `OUT`: The per-package output directory.
*   `SRC`: The per-package source tree.
*   `T`: The per-package temporary directory shared with the ebuild.

While `/mnt/host/source/` is mounted in for test frameworks, unit tests should
never utilize that path directly.

## Network interaction

Some networking functionality is available to unit tests. No connections to the
outside world is permitted of course (e.g. no DNS access), but tests may create
local servers to validate communication.

When running unit tests, a new
[network namespace](https://man7.org/linux/man-pages/man7/network_namespaces.7.html)
is created for each package. This means `localhost` with nothing else running
(i.e. all ports are available). Tests should still try to avoid hardcoding
specific ports since a particular package might have many unit tests executing
in parallel, but they won't be contending with other packages or normal system
services.

Keeping the [Kernel interfaces] guidelines in mind, only assume basic common
network functionality is available. Other protocols and functionality should be
a VM integration test instead.

Some high level guarantees unit tests can assume:

*   IPv4 & IPv6 are available
*   `localhost` is resolvable
    *   `127.0.0.1/8` works for IPv4
    *   `::1/128` works for IPv6
*   Common [socket](https://man7.org/linux/man-pages/man2/socket.2.html) APIs
    may be used
    *   Only these address families may be used
        *   [`AF_INET`](https://man7.org/linux/man-pages/man7/ip.7.html)
        *   [`AF_INET6`](https://man7.org/linux/man-pages/man7/ipv6.7.html)
        *   [`AF_UNIX`](https://man7.org/linux/man-pages/man7/unix.7.html) may
            be used to create new temporary sockets, but no external services
            (e.g. syslog) are accessible
        *   [`AF_NETLINK`](https://man7.org/linux/man-pages/man7/netlink.7.html)
            may be used to create local connections and modify the current
            network namespace, but no external resources may be accessed
        *   [`AF_PACKET`](https://man7.org/linux/man-pages/man7/packet.7.html)
    *   Only these types may be used
        *   `SOCK_STREAM` (e.g. [TCP](https://man7.org/linux/man-pages/man7/tcp.7.html))
        *   `SOCK_DGRAM` (e.g. [UDP](https://man7.org/linux/man-pages/man7/udp.7.html))
        *   [`SOCK_RAW`](https://man7.org/linux/man-pages/man7/raw.7.html)
        *   `SOCK_PACKET` (c.f. `AF_PACKET` above).

## Device interaction (/dev)

Per the [Kernel interfaces] guidelines, access to `/dev` resources is heavily
restricted. Only the most basic & stable pseudo interfaces are permitted:

*   `/dev/fd/` `/dev/stdin` `/dev/stdout` `/dev/stderr`: Normal standard I/O.
*   `/dev/null`: The [null device](https://man7.org/linux/man-pages/man4/null.4.html).
*   `/dev/full`: The [full device](https://man7.org/linux/man-pages/man4/full.4.html).
*   `/dev/zero`: The [zero device](https://man7.org/linux/man-pages/man4/zero.4.html).
*   `/dev/urandom`: The [urandom device](https://man7.org/linux/man-pages/man4/urandom.4.html).
*   `/dev/tty`: The [tty device](https://man7.org/linux/man-pages/man4/tty.4.html).
*   `/dev/ptmx` `/dev/pts/`: [Pseudo terminals](https://man7.org/linux/man-pages/man4/pts.4.html).
*   `/dev/shm/`: [POSIX shared memory](https://man7.org/linux/man-pages/man7/shm_overview.7.html).
*   `/dev/loop#` `/dev/loop-control`: [Loopback devices](https://man7.org/linux/man-pages/man4/loop.4.html).

If access to other interfaces are required, the test has to run in a VM instead.
Some notable examples are
[syslog (`/dev/log`)](https://man7.org/linux/man-pages/man3/syslog.3.html),
[FUSE (`/dev/fuse`)](https://www.kernel.org/doc/html/v6.1/filesystems/fuse.html),
and [TUN/TAP](https://www.kernel.org/doc/html/v6.1/networking/tuntap.html).
Hardware interfaces like USB or PCI buses are definitely not permitted.

## Procfs interaction (/proc)

Unit tests run in unique
[PID namespaces](https://man7.org/linux/man-pages/man7/pid_namespaces.7.html)
to isolate them from other packages and the build system itself. A new PID ns
is created for each package, but not each individual unit test.

The [Linux procfs](https://man7.org/linux/man-pages/man5/proc.5.html) contains
basic process information, but also a ton of system settings and runtime
configuration overrides (e.g.
[sysctl](https://man7.org/linux/man-pages/man8/sysctl.8.html)). The runtime
settings are not isolated from the build system kernel and so must never be
modified on the fly.

The unit test environment has many such paths stubbed out or disabled completely
during execution. Only the most stable (and read-only) interfaces are passed in.

## Sysfs interaction (/sys)

Per the [Kernel interfaces] guidelines, access to `/sys` resources is heavily
restricted. Only the most basic & stable pseudo interfaces are permitted. The
exact list is still being developed, but keep in mind that no hardware paths
may be inspected as these would reflect the build system itself.

## Dealing with time

### Faking "now"

Sometimes code needs to know the current time. Imagine the following class:

```c++
// stopwatch.h:

// Measures how long an operation takes.
class Stopwatch {
  ...

  // Records the start of the operation.
  void StartOperation();

  // Returns the elapsed time since the operation started.
  base::TimeDelta EndOperation();

 private:
  // The time at which StartOperation() was called.
  base::TimeTicks start_time_;
  ...
};

// stopwatch.cc:

void Stopwatch::StartOperation() {
  start_time_ = base::TimeTicks::Now();
}

base::TimeDelta Stopwatch::EndOperation() {
  return base::TimeTicks::Now() - start_time_;
}
```

Note that this code uses `base::TimeTicks` and `base::TimeDelta` to hold
time-related values rather than `time_t` and integers; the former types are
efficient and self-documenting (e.g. no confusion about milliseconds vs.
seconds).

It's still difficult to test, though: how does one verify that the class
accurately measures time? Resist the temptation to call `sleep()` from within
tests; short delays add up when many tests are run, and build systems are often
overloaded, making it impossible to predict how long an operation will take and
avoid flakiness when tests depend on the passage of time.

Instead, use `base::Clock` and `base::TickClock` to get the current time:

```c++
// stopwatch.h:

class Stopwatch {
 public:
  // Ownership of |clock| remains with the caller.
  explicit Stopwatch(base::TickClock* clock);
  ...

 private:
  base::Clock* clock_;  // Not owned.
  ...
};

// stopwatch.cc:

Stopwatch::Stopwatch(base::TickClock* clock) : clock_(clock) {}

void Stopwatch::StartOperation() {
  start_time_ = clock_->NowTicks();
}

base::TimeDelta Stopwatch::EndOperation() {
  return clock_->NowTicks() - start_time_;
}

// main.cc:

int main(int argc, char** argv) {
  base::DefaultClock clock;
  Stopwatch stopwatch(&clock);
  stopwatch.StartOperation();
  ...
  const base::TimeDelta duration = stopwatch.EndOperation();
  ...
}
```

Now tests can pass in their own `base::SimpleTestTickClock` and adjust the time
that it returns as "now":

```c++
TEST_F(StopwatchTest, MeasureTime) {
  base::SimpleTestTickClock clock;
  Stopwatch stopwatch(&clock);
  stopwatch.StartOperation();

  constexpr base::TimeDelta kDelay = base::TimeDelta::FromSeconds(5);
  clock.Advance(kDelay);
  EXPECT_EQ(kDelay, stopwatch.EndOperation());
}
```

### Asynchronous tasks

Sometimes, the code that's being tested does work asynchronously. Imagine a
function that posts a task that performs some work and runs a callback:

```c++
// Starts work. |callback| is run asynchronously on completion.
void Start(const base::Closure& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&DoWork, callback);
}

// Performs work on behalf of Start() and runs |callback|.
void DoWork(const base::Closure& callback) {
  ...
  callback.Run();
}
```

A test can use `base::RunLoop` to run the message loop asynchronously and verify
that the callback was executed:

```c++
namespace {

// Increments the value pointed to by |num_calls|. Used as a callback for
// Start().
void RecordCompletion(int* num_calls) { *num_calls++; }

}  // namespace

TEST(CallbackIsRun) {
  int num_calls = 0;
  Start(base::Bind(&RecordCompletion, base::Unretained(&num_calls)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, num_calls);
}
```

Alternately, the callback can be used to stop the message loop:

```c++
TEST(CallbackIsRun) {
  base::RunLoop run_loop;
  Start(run_loop.QuitClosure());
  run_loop.Run();
}
```

Also consider forgoing `base::MessageLoop` entirely in cases like this and
instead injecting a `base::TaskRunner` so tests can control where tasks are run.

### Multithreaded code

Multithreaded code can be challenging to test reliably. Avoid sleeping within
the test to wait for other threads to complete work; as described above, doing
so both makes the test slower than necessary and leads to flakiness.

It's often best to hide threading details by structuring your code to post
callbacks back to the calling message loop. See
`base::TaskRunner::PostTaskAndReply()` and the "Asynchronous tasks" section
above.

If that isn't possible, tests can use `base::WaitableEvent` or other
synchronization types to wait just as long as is necessary.

## Avoid #ifdefs when possible

ChromeOS supports many different hardware configurations. Much of the
variability is described at build-time using Portage USE flags. A common pattern
is to map these USE flags to C preprocessor macros and then test them within C++
code using `#ifdef` or `#if defined(...)`.

When this is done, only one specific configuration is compiled and tested when
the package is built for a given board. If a test wants to exercise code that's
only enabled for a subset of configurations, the test must be protected with a
similar preprocessor check. If a developer is modifying the code, they must
perform builds and run unit tests for multiple boards to make sure they haven't
broken any supported configurations.

It's often better to attempt to keep as much of the code as is possible testable
when a build is performed for a single board. This makes development much
easier: even if the board that the developer is using corresponds to a
Chromebook, they can test Chromebox-specific code, for example.

Instead of sprinkling preprocessor checks across the codebase, store the
configuration parameters in a struct or class that can be constructed and
injected by unit tests to exercise different configurations. If preprocessor
checks are necessary, constrain them to a single location that's not compiled
into tests (e.g. the file containing the `main` function). If the code requires
extensive configuration, consider introducing configuration or preference files
(which can be written by tests).

Preprocessor checks can also be made more readable by hiding them within
functions instead of distributing them throughout the code:

```c++
inline bool HasKeyboard() {
#if defined(DEVICE_HAS_KEYBOARD)
  return true;
#else
  return false;
#endif
}

void DoSomething() {
  if (HasKeyboard()) {
    ...
  } else {
    ...
  }
}
```

This pattern also makes it easier to move away from preprocessor-based
configuration in the future.

## Use Google Mock judiciously

[Google Mock] (formerly "gmock", and now part of [Google Test]) is "an extension
to Google Test for writing and using C++ mock classes". It can be used to create
mock objects that return canned values and verify that they're called in an
expected manner. This can be done with minimal changes to the code that's being
tested -- if a class has virtual methods, you can use Google Mock to create a
partial mock of it. To accomplish this, Google Mock introduces what's
essentially a new language on top of C++, heavily dependent on preprocessor
macros and template magic. As a result, tests that use Google Mock can be very
difficult to read, write, and debug. Incorrectly using Google Mock macros can
result in very long and confusing compilers errors due to its heavy use of
templates.

Google Mock can be useful in some cases. For example, testing that methods
across multiple objects are invoked with the right arguments and in the correct
order is much easier to do using Google Mock than with a roll-your-own approach.

However, Google-Mock-based tests are often brittle and difficult to maintain.
Tests that use Google Mock frequently verify behavior that's not directly
related to the functionality being tested; when refactoring code later, it then
becomes necessary to make changes to unrelated tests. On the other hand, if
tests instead underspecify the behavior of mocked objects, Google Mock can
produce large amounts of log spam (e.g. `Uninteresting mock function call`
warnings that are logged when a mock's return values are not specified).

Don't use Google Mock to paper over code that wasn't designed with testability
in mind. Instead, modify the code so that you can add the tests cleanly. Define
and use clear, pure-virtual interfaces wherever possible. Feel free to use
Google Mock to define mock implementations of those interfaces if doing so makes
your tests shorter and more readable, but don't use it to mock concrete classes,
especially when those classes are what you're trying to test.

See the 2012 ["Should we officially deprecate GMock?"] and 2016 ["Using
NiceMock?"] chromium-dev mailing list threads for more discussion. The ["Friends
You Can Depend On"] and ["Know Your Test Doubles"] posts on the [Google Testing
Blog] also include more information about different types of test objects.

### Best Practices

*   Use [`MOCK_METHOD`] macros instead of `MOCK_METHOD1`, `MOCK_METHOD2`, etc.
*   Use `override` in `MOCK_METHOD` on a virtual method so that you get a
    compiler error if the declarations in the class and mock do not match.

***note
**NOTE:** `clang-format` sometimes formats `Bar* bar` as `Bar * bar` in
 `MOCK_METHOD`. See https://crbug.com/1006323.
***

```c++
class Task {
 public:
  Task() = default;
  virtual ~Task() = default;
  virtual bool IsDone() const = 0;
  virtual bool DoIt(const std::map<int, int>& arguments,
                    std::string* result,
                    const base::Closure& error_callback) = 0;
};

// Old way
class MockTask : public Task {
 public:
  MOCK_CONST_METHOD0(IsDone, bool());
  MOCK_METHOD3(DoIt,
               bool(const std::map<int, int>&,
                    std::string*,
                    const base::Closure&));
  MOCK_METHOD0(OnError, void());
};

// New preferred way
class MockTask : public Task {
 public:
  MOCK_METHOD(bool, IsDone, (), (const, override));  // NOTE: const and override
  MOCK_METHOD(bool,
              DoIt,
              ((const std::map<int, int>&), std::string*, const base::Closure&),
              (override));  // NOTE: override specified
  MOCK_METHOD(void, OnError, ());
};
```

## Code coverage

The general idea is to run
`USE=coverage FEATURES="test noclean" emerge-<BOARD> <PKG>`, which runs the
unit tests and creates a html report which can be viewed on the build machine.

```bash
USE=coverage FEATURES="test noclean" emerge-nocturne biod
# Check for line containing the report near the end of build logs
# that would look like:
# Coverage report for biod generated at file:///<path/to/chroot>/
# chroot/build/nocturne/build/coverage_data/chromeos-base/biod-0/<version>/index.html
```

If the above invocation does not work for any reason, the following instructions
might be useful. Note that these instructions assume that your package uses Clang.
Code coverage using clang needs the following two artifacts which are
automatically generated when running with USE=coverage:
-   The llvm profiling data, `default.profraw`, which might be found in the
    package's source directory (the CWD where unit test ran).
-   The test executable(s), which may be found in a directory under
    `/build/<BOARD>/tmp/portage/<FULL_PKG_PATH>-9999/work/build/out/Default/`.
    One way to find the executable would be to determine the unit test
    executable name from the package's BUILD.gn file and use the find utility.

Once you have identified the two artifacts, you can analyze the code
coverage using `llvm-cov`.

Here is an example of this workflow for the biod package.

```bash
export BOARD=nocturne
USE=coverage cros build-packages --board=$BOARD --skip-chroot-upgrade biod
cros_workon --board=$BOARD start biod
LLVM_PROFILE_FILE="/tmp/default.profraw" USE=coverage FEATURES="test noclean" emerge-$BOARD biod
cd /build/$BOARD/tmp/
llvm-profdata merge -o default.profdata default.profraw

# Now inspect the ~/chromiumos/src/platform2/biod/BUILD.gn file to
# determine the test executable name, which is biod_test_runner in this case.
# Next, search /build/$BOARD/tmp/portage for that binary name.

# To generate an HTML summary with line-by-line reporting, use the following:
llvm-cov show -instr-profile=default.profdata -format=html -output-dir=. -object=/build/$BOARD/tmp/portage/chromeos-base/biod-9999/work/build/out/Default/biod_test_runner
# Now, point your web browser to ~/chromiumos/chroot/build/$BOARD/tmp/index.html

# To show console based coverage metrics per file, use the following:
llvm-cov report -instr-profile=default.profdata -object=/build/$BOARD/tmp/portage/chromeos-base/biod-9999/work/build/out/Default/biod_test_runner
```

[Unit tests]: https://en.wikipedia.org/wiki/Unit_testing
[Google Test]: https://github.com/google/googletest
[unit testing document]: /chromium-os/developer-library/guides/testing/running-unit-tests/#Adding-unit-tests
[Functional tests]: https://en.wikipedia.org/wiki/Functional_testing
[Tast]: https://chromium.googlesource.com/chromiumos/platform/tast/
[Autotest]: https://chromium.googlesource.com/chromiumos/third_party/autotest/+/HEAD/docs/user-doc.md
[commit queue]: https://www.chromium.org/developers/testing/commit-queue/design/
[Google Mock]: https://github.com/google/googletest/blob/HEAD/googlemock/README.md
[Google Test]: https://github.com/google/googletest
["Should we officially deprecate GMock?"]: https://groups.google.com/a/chromium.org/forum/#!msg/chromium-dev/-KH_IP0rIWQ/HynALJ3rsk0J
["Using NiceMock?"]: https://groups.google.com/a/chromium.org/forum/#!msg/chromium-dev/1zktlTqFb6o/g5-Llb7LCgAJ
["Friends You Can Depend On"]: https://testing.googleblog.com/2008/06/tott-friends-you-can-depend-on.html
["Know Your Test Doubles"]: https://testing.googleblog.com/2013/07/testing-on-toilet-know-your-test-doubles.html
[Google Testing Blog]: https://testing.googleblog.com/
[`MOCK_METHOD`]: https://github.com/google/googletest/blob/HEAD/docs/gmock_cook_book.md#creating-mock-classes
