---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - Chromium OS > Developer Library > Guides
page_name: python-mock
title: Python unittest mocking
---

The python "mock" library is the preferred mocking library for python unittests
in the ChromiumOS source.  There still exist many tests that use "mox", but
"mock" is preferred for all new tests.  python-mock is the standard mocking
framework starting in Python 3.3 (imported as `unittest.mock`).

## Setup

`python-mock` is already added to the chroot, but people who run unit tests
outside the chroot need to manually install it. Here are the commands to run:

```bash
sudo apt-get purge python-mock
sudo apt-get install python-pip
sudo pip install mock
```

## Summary

Unlike pymox, python-mock is really easy to extend - no messing with
`ReplyAll()`'s, `VerifyAll()`s, etc.  It's also more pythonic.  Here's a quick
snippet of running a test with `somemodule.func()` mocked out:

```python
import somemodule

with patch.object(somemodule, 'func') as mocked_f:
  mocked_f.returnvalue = 5
  ret = somemodule.func('arg1', 'arg2')
  assertEquals(ret, 5)
  mocked_f.assert_called_once_with('arg1', 'arg2')
```

## Documentation

*   Take a look at python-mock [docs].
*   There's also a [talk] given by the author on how to test with python-mock.

### partial\_mock.py

In the chromite source there is a partial mocking library, located at
`lib/partial_mock.py`.  This extends python-mock to allow for writing
re-usable mocks.  An example is the `cros_build_lib_unittest.RunCommandMock`
class, which can be used to easily simulate command line behavior.
Here's a code snippet of a function we want to test:

```python
def func():
  try:
    result = cros_build_lib.run(['./some_command'], stdout=True, stderr=True)
    return 'passcode' in result.stdout
  except RunCommandError as e:
    if (e.result.returncode == 2 and
        'monkey' in e.result.stderr):
      return False
    raise
```

Here's how we'd test it with the RunCommandMock partial mock:

```python
from cros_build_lib_unittest import RunCommandMock, RunCommandError

with RunCommandMock() as pmock:
  pmock.AddCmdResult(['./some_command'], returncode=2, stderr='12 monkeys')
  assertEquals(func(), False)
  pmock.AddCmdResult(['./some_command'], returncode=1)
  assertRaises(RunCommandError, func)
  pmock.AddCmdResult(['./some_command'], stdout='passcode')
  assertEquals(func(), True)
```

No mucking with setting up mock `CommandResults`, or mock `RunCommandErrors`, or
testing for the right `redirect_*` or `error_code_ok=True` arguments. The
partial mock handles all of that for you by using `run` logic directly.

It's also easy to write your own partial mocks that can then be used by other
unit tests.  Having a corpus of re-usable mocks will make the barrier to testing
lower, and will help us get into a more test-driven development workflow.

Thanks for reading, and happy mocking!

[docs]: https://docs.python.org/3/library/unittest.mock.html
[talk]: https://pyvideo.org/pycon-us-2011/pycon-2011--testing-with-mock.html
