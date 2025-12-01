# Google Cloud Storage usage in Chromium

This doc explains the components that help Chromium developer's and Chromium
infrastructure interact with Google Cloud Storage.

## When will I encounter Google Cloud Storage when developing Chromium

There may be different hooks run by `gclient` that pull necessary artifacts
from Google Cloud Storage. `gclient runhooks` or `gclient sync` command would
download the files, depending on your `.gclient` config.

In other cases the Chromium CI build recipes also read and write objects to
Google Cloud Storage.

## Components

These components should have the same effect whether they're run on a Swarming
bot or a developer's Chromium checkout. These components *do not* affect the
usage of the standard `gsutil` or `gcloud storage` binaries outside of Chromium
workflows. In those cases, you would need to use `gsutil config` or
`gcloud auth login` directly.

### gsutil.py

[gsutil.py](https://source.chromium.org/chromium/chromium/tools/depot_tools/+/main:gsutil.py)
wraps an unmodified version of [gsutil](https://cloud.google.com/storage/docs/gsutil)
which handles authentication support with LUCI Auth and pins the gsutil version.

All `gsutil` arguments, except for `config`, are passed through to the actual
`gsutil` binary. The `config` argument will redirect to a standard
`luci-auth login` interactive flow and generate a token with
[https://www.googleapis.com/auth/devstorage.full_control](https://cloud.google.com/storage/docs/oauth-scopes)
scope. This scope is necessary for the wide range of commands gsutil supports.

Every command sent through to the gsutil binary is wrapped with
`luci-auth context`. Context starts a local LUCI auth context server which
creates a temporary BOTO file which in turn fools gsutil into fetching its
access tokens from the context server. When the command is finished the BOTO
file is deleted and the local context server stopped so there's nothing to
persist and risk leaking. *Note: The context server is always running on bots.*

You can view this happening here:

```sh
root@8447532ea598:~/chromium$ inotifywait -m -r /tmp/ | grep .boto
Setting up watches.
Watches established.
/tmp/luci56666486/gsutil-luci-auth/ OPEN .boto
/tmp/luci56666486/gsutil-luci-auth/ ACCESS .boto
/tmp/luci56666486/gsutil-luci-auth/ CLOSE_NOWRITE,CLOSE .boto
/tmp/luci56666486/gsutil-luci-auth/ OPEN .boto
/tmp/luci56666486/gsutil-luci-auth/ ACCESS .boto
/tmp/luci56666486/gsutil-luci-auth/ CLOSE_NOWRITE,CLOSE .boto
/tmp/luci56666486/gsutil-luci-auth/ DELETE .boto
```

The code that actually performs this can be found below in the
[LUCI auth gsutil integration](#luci-auth-gsutil-integration) section.

Note: `gsutil.py` can use any `.boto` file provided in the home directory or via
the `BOTO_CONFIG` environment variable. You should not need either of these in
most cases, furthermore they will likely cause problems for you so we strongly
suggest removing them if already set.

### download_from_google_storage.py

[download_from_google_storage.py](https://source.chromium.org/chromium/chromium/tools/depot_tools/+/main:download_from_google_storage.py)
is a convenience wrapper library around [gsutil.py](https://source.chromium.org/chromium/chromium/tools/depot_tools/+/main:gsutil.py).
If you are not working in Python then it's probably best to use the [gsutil.py](#gsutilpy)
command-line interface (CLI) described above.

### LUCI Auth gsutil Integration

[luci-auth gsutil integration](https://pkg.go.dev/go.chromium.org/luci/auth/integration/gsutil)
library is a shim library that's used to enable a 3-legged OAuth flow using LUCI
auth refresh tokens rather than the standard `gcloud auth login` refresh
tokens or `gsutil config` generated BOTO file.
