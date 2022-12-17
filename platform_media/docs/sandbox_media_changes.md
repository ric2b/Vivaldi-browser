# Sandbox and OS media API

## [**Back to top**](../README.md)

## Overview

Using OS API to decode audio and video requires to adjust sandboxes
Chromium uses for various processes. In particular, the sandboxes for
the renderer, GPU and utility processes must permit calls to the
corresponding OS libraries.

## Mac

On Mac the sandbox is relaxed using scripts in
`platform_media/sandbox/mac`.

TODO: Presently this does not change the sandbox for the utility
process, but Chromium can call FFmpeg audio decoders from it so perhaps
the sandbox should be adjusted for this process as well.

## Windows

On Windows our code preloads media-related DLLs before the sandbox is
initialized, see `platform_media/sandbox/win`. Then instead of loading
DLLS directly the code uses preloaded handles of those DLLS. To get
overview which processes are affected, search the code for
`platform_media_init` string which is C++ namespace for the relevant
code.