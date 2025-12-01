# depot_tools

Tools for working with Chromium development. It requires python 3.8.


## Tools

The most important tools are:

- `fetch`: A `gclient` wrapper to checkout a project. Use `fetch --help` for
  more details.
- `gclient`: A meta-checkout tool. Think
  [repo](https://source.android.com/source/using-repo.html) or [git
  submodules](https://git-scm.com/docs/git-submodule), except that it support
  OS-specific rules, e.g. do not checkout Windows only dependencies when
  checking out for Android. Use `gclient help` for more details and
  [README.gclient.md](README.gclient.md).
- `git cl`: A code review tool to interact with Rietveld or Gerrit. Use `git cl
  help` for more details and [README.git-cl.md](README.git-cl.md).
- `roll-dep`: A gclient dependency management tool to submit a _dep roll_,
  updating a dependency to a newer revision.

There are a lot of git utilities included.

Also, includes shell script/batch file for tools required to build chromium,
e.g.

- `gn`: a meta-build system that generates build files for Ninja
- `autoninja`: a wrapper for `siso` and `ninja`.
- `siso`: a build tool that aims to significantly speed up Chromium's build.
- `ninja`: a small build system with a focus on speed.  deprecated by Siso.

These shell script/batch file runs python script with `python-bin/python3`
that find binaries in chromium checkout, and run with proper setup/check.
To use these wrappers, you need to initialize/bootstrap depot_tools (using
`gclient`, `update_depot_tools` or `ensure_bootstrap`).

## Installing

See [set-up documentation](https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up).

depot_tools is also available in

- chromium's third_party/depot_tools:
  propagated by [autoroller](https://autoroll.skia.org/r/depot-tools-chromium-autoroll).

- on builder:
  [infra_internal/recipe_bundles/chrome-internal.googlesource.com/chrome/tools/build](https://chrome-infra-packages.appspot.com/p/infra_internal/recipe_bundles/chrome-internal.googlesource.com/chrome/tools/build) bundles depot_tools.
  propagated by [build_internal recipe roller](https://ci.chromium.org/ui/p/infra-internal/builders/cron/build_internal%20recipe%20roller)

These depot_tools would not be initialized/bootstrapped (i.e. no
`python-bin/python3` binary available), so the build tool wrapper won't work,
unless it is explicitly initialized by `ensure_bootstrap`.
Or, directly call the python script instead of using the shell script/batch
file.


## Updating

`depot_tools` updates itself automatically when running `gclient` tool. To
disable auto update, set the environment variable `DEPOT_TOOLS_UPDATE=0` or
run `./update_depot_tools_toggle.py --disable`.

To update package manually, run `update_depot_tools.bat` on Windows,
or `./update_depot_tools` on Linux or Mac.

Running `gclient` will install `python3` binary.


## Contributing

To contribute change for review:

    git new-branch <somename>
    # Hack
    git add .
    git commit -a -m "Fixes goat teleporting"
    # find reviewers
    git cl owners
    git log -- <yourfiles>

    # Request a review.
    git cl upload -r reviewer1@chromium.org,reviewer2@chromium.org --send-mail

    # Edit change description if needed.
    git cl desc

    # If change is approved, flag it to be committed.
    git cl set-commit

    # If change needs more work.
    git rebase-update
    ...
    git cl upload -t "Fixes goat teleporter destination to be Australia"

See also [open bugs](https://issues.chromium.org/issues?q=status:open%20componentid:1456102),
[open reviews](https://chromium-review.googlesource.com/q/status:open+project:chromium%252Ftools%252Fdepot_tools),
[forum](https://groups.google.com/a/chromium.org/forum/#!forum/infra-dev) or
[report problems](https://issues.chromium.org/issues/new?component=1456102).

### cpplint.py

Until 2018, our `cpplint.py` was a copy of the upstream version at
https://github.com/google/styleguide/tree/gh-pages/cpplint. Unfortunately, that
repository is not maintained any more.
If you want to update `cpplint.py` in `depot_tools`, just upload a patch to do
so. We will figure out a long-term strategy via issue https://crbug.com/916550.
