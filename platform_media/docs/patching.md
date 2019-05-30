# Patching

## [**Back to top**](../README.md)

## Introduction

An important part of proprietary media in Vivaldi is the patches, this is because we require a quite large set of patches to be able to delegate decoding to the platform_media module.

There are three patches - each serving a separate function :

1. MANUALMERGE - PART 0 : Add the Opera Proprietary Media Patches
2. MANUALMERGE - PART 1 : Add the Opera Proprietary Media Patches
3. MANUALMERGE - PART 2 : Add the Opera Proprietary Media Patches

The distribution and impact of the patches are illustrated in this diagram, where it's clear that the majority of the patches are to the media subdirectory closely followed by content, and the rest of the impacted subdirectories only have a few patched files each.

![Patches Overview](images/patches.svg)

## Patch 0 : Minimal patch to build the module

The goal of this patch is to make it easier to do the media patching. Once this patch is applied the platform_media module should compile. Patch-process-wise this means that when you have applied this patch and turned on media you will see any compile errors resulting from changes in APIs in chromium in this version.

## Patch 1 : Master patch for platform media

This patch contains most of the patches needed to make proprietary media work in Vivaldi. This is also where you are most likely to see conflicts. There are normally conflicts in between 4-8 files when applying this patch.
The conflicts are generally not hard to resolve if you look at the diff from the previous version of Vivaldi. Also check for moved files if you find that one of the files we have patches on is removed.

## Patch 2 : Force access to GPU when blacklisted

Large parts of platform_media runs in the GPU process, even though it doesn’t itself use the GPU. Chromium has however blacklisted some GPUs completely and will not start the GPU process at all. This patch forces access to the GPU process even if the actual GPU is blacklisted. It forces this access only with regards to calls from platform_media.
