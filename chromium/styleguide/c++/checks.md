# CHECK(), DCHECK(), NOTREACHED_NORETURN() and NOTREACHED_IN_MIGRATION()

`CHECK()`, `DCHECK()`, `NOTREACHED_NORETURN()` and `NOTREACHED()` are all used
to ensure that invariants hold.  They document (and verify) programmer
expectations that either some statement *always* holds true at the point of
`(D)CHECK`ing or that a piece of code is unreachable. They should not be used to
validate data that is provided by end-users or website developers. Such data is
untrusted, and must be validated by standard control flow.

An invariant that does not hold should be seen as Undefined Behavior, and
continuing past it puts the program into an unexpected state. This applies in
particular to `DCHECK()` and `NOTREACHED()` since they do not test anything in
production and thus do not stop the program from continuing with the invariant
being violated. All invariant failures should be seen as P1 bugs, regardless of
their crash rate. Continuing past an invariant failure can cause crashes and
incorrect behaviour for our users, but also frequently presents security
vulnerabilities as attackers may leverage the unexpected state to take control
of the program. In the future we may let the compiler assume and optimize around
`DCHECK()`s holding true in non-DCHECK builds using `__builtin_assume()`, which
further formalizes undefined behavior.

## Failures beyond Chromium's control

Failure can come from beyond Chromium's ability to control. These failures
should not be caught with invariants, but handled as part of regular control
flow. In the rare case where it's impossible to safely recover from failure use
`base::ImmediateCrash()` to terminate the process instead of using `CHECK()`
etc. Doing so avoids implying that the generated crash reports should be triaged
as bugs in Chromium. Fatally aborting is a last-resort measure.

We must be resilient to a bad prior release of Chromium which may have persisted
bad data to disk or a bad server-side rollout which may be sending us incorrect
or unexpected configs.

Note that wherever `CHECK()` is inappropriate, `DCHECK()` is inappropriate as
well. `DCHECK()` should still only be used for invariants. Ideally we'd have
better test coverage for failures created from outside Chromium's control.

Non-exhaustive list of failures beyond Chromium's control:

* *Exhausted resources:* Running out of memory, FD handles, etc. should be made
  unlikely to happen, but is not entirely within our control. When we can't
  gracefully degrade, use a non-asserting `base::ImmediateCrash()`.
* *Untrusted data:* Data provided by end users or website developers. Don't
  `CHECK()` for bad syntax, etc.
* *Serialized data out of sync with binary:* Any data persisted to disk may come
  from a past or future version of Chrome. Server data such as experiments
  should also not be verified with `CHECK()`s as a bad server-side rollout
  shouldn't be able to bring down the client. Note that you may `CHECK()` that
  data is valid after the caller should've validated it.
* *Disk corruption:* We should be able to recover from a bad disk read/write. Do
  not assume that the data comes from a current (or even past) version of
  Chromium. This includes preferences which are persisted to disk.
* *Data across security boundaries:* A compromised renderer should not be able
  to bring down the browser process (higher privilege). Bad IPC messages should
  be safely [rejected](../../docs/security/mojo.md#explicitly-reject-bad-input)
  by Chromium without the use of `base::ImmediateCrash()` or `CHECK()` etc.
  as part of normal control flow.
* *Bad/untrusted/changing driver, kernel API, hardware failure:* A misbehaving
  GPU driver may cause us to be unable to proceed. This is not an invariant
  failure. On platforms where we are wary that a kernel API may change without
  sufficient prior notice we should not `CHECK()` its result as we expect the
  rug to be pulled from under our feet. In the case of hardware failure we
  should not for instance `CHECK()` that a write succeeded.

In some cases (malware, ..., dll injection, etc.) third-party code outside of
our control can cause us to trigger various CHECK() failures, due to injecting
code or unexpected state into Chrome. These can create "weird machines" that are
useful to attackers. We don't remove CHECKs just to support them, though we may
handle these unexpected states if possible and necessary. Chromium is not
designed to run against arbitrary code modification.

## Invariant-verification mechanisms

Prefer `CHECK()` and `NOTREACHED_NORETURN()` as they ensure that if an invariant
fails, the program does not continue in an unexpected state, and we hear about
the failure either through a test failure or a crash report. This helps prevent
user harm such as security bugs when our software does what we did not expect.
Historically, `CHECK()` was seen as expensive but great effort and care has gone
into making the crash instructions nearly free on modern CPUs. Log messages are
discarded from `CHECK()`s in production builds but provide additional
information in debug and `DCHECK` builds.

`DCHECK()` (and `DCHECK_EQ()`, `DCHECK_LT()`, etc) provide a fallback mechanism
to check for invariants where the test being performed is too expensive (either
in terms of generated code size or performance) to verify in production builds.
The risk of depending on `DCHECK()` is that, since it disappears in production
builds, it only exists in tests, on developer machines and a very small subset
of Canary builds. Any side effects intended to happen inside the `DCHECK()`
disappear from production along with it, and unexpected behaviour can happen
afterward as a result.

`NOTREACHED_NORETURN()` signals that a piece of code is intended to be
unreachable, and lets the compiler optimize based on that fact, while
terminating if it is in fact reached. Like `CHECK()`, this ensures we hear about
the invariant failing through a test failure or a crash report, and prevents
user harm. Historically we used the shorter `NOTREACHED()` to indicate code was
unreachable, however it disappears in production builds and we have observed
that these are in fact commonly reached. Prefer `NOTREACHED_NORETURN()` in new
code, while we migrate the preexisting cases to it with care. See
https://crbug.com/851128.

## Examples

Below are some examples to explore the choice of `CHECK()` and its variants:

```c++
// Good:
//
// Testing pointer equality is very cheap so write this as a CHECK. A security
// bug would happen afterward if the CHECK fails (in this case, on the next
// line).
auto it = container.find(key);
CHECK(it != container.end());
*it = Foo();

// Good:
//
// This is an expensive operation. Consider writing a test to provide coverage
// for this as well. DCHECK() is available as a fallback to verify the condition
// in tests and on a small subset of Canary builds.
DCHECK(|invoke an O(n^2) operation|);

// Good:
//
// This switch handles all cases, but enums can technically hold any integer
// value (even if all enum members are enumerated), so the compiler must try to
// handle other cases too. We can avoid dealing with values outside enums by
// using NOTREACHED_NORETURN() while also making sure we hear about it.
switch (my_enum) {
  case A: return 1;
  case B: return 5;
  case C: return 3;
}
NOTREACHED_NORETURN();

// Bad:
//
// Do not handle `DCHECK()` failures. Use `CHECK()` instead and omit the
// conditional below.
DCHECK(foo);  // Use CHECK() instead and omit conditional below.
if (!foo) {
  ...
}

// Bad:
//
// Use CHECK(bar); instead.
if (!bar) {
  NOTREACHED_IN_MIGRATION();
  return;
}
```

## More cautious CHECK() rollouts and DCHECK() upgrades

If you're not confident that an unexpected situation can't happen in practice,
an additional `base::NotFatalUntil::M120` argument after the condition may be
used to gather non-fatal diagnostics before turning fatal in a future milestone.
Make sure to either prioritize these invariant failures once discovered, and
punt the milestone where this invocation turns fatal to avoid rolling out a
stability risk. Macros with a `base::NotFatalUntil` argument will provide
non-fatal crash reports before the fatal milestone is hit. They preserve and upload logged arguments that are uploaded along the report which is useful
for debugging failures as well.

This extra argument can be used to more cautiously add or upgrade to `CHECK()`s.
This is appropriate when we're uncertain of whether the invariant currently
holds true or when there's low pre-stable coverage. Specifically consider using
these:

* When working on iOS code (low pre-stable coverage).
* Upgrading `DCHECK()s`.
* Working on code that's not flag guarded.

As `base::NotFatalUntil` automatically turns fatal, keep an extra eye on
automatically-filed bugs for failures in the wild. Discovered failures, like
other invariant failures, are high-priority issues. Once resolved, either by
handling the unexpected situation or making sure it no longer happens, the
milestone number should be bumped to allow for validation in stable channels
before turning fatal.

Failing instances should not downgrade to DCHECKs as that hides the ongoing
problem rather than resolving it. In rare exceptions you could use
`DUMP_WILL_BE_CHECK()` macros for similar semantics (report on failure) without
timeline expectations, though in this case you must also handle failure as best
you can as failures are known to happen.

## Alternatives in tests

For failures in tests, GoogleTest macros such as `EXPECT_*`, `ASSERT_*` or
`ADD_FAILURE()` are more appropriate than `CHECKing`. For production code:

* `LOG(DFATAL)` is fatal on bots running tests but only logs an error in
  production.
* `DLOG(FATAL)` is fatal on bots running tests and does nothing in production.

As these only cause tests to fail, they should be rarely used, and mostly exist
for pre-existing code. Prefer to write a test that covers these scenarios and
verify the code handles it, or use a fatal `CHECK()` to actually prevent the
case from happening.
