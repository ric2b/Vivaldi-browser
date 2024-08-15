---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: user-sessions
title: User Sessions
---
[TOC]

This page provides an overview of Chrome user sessions, which are a key
part of ChromeOS development, especially for features with lifetimes tied to
when users sign in.

## Chrome user sessions

### Profiles

A profile is a bundle of data about the current user and the current chrome
session that can span multiple browser windows, and profiles are used to
separate Chrome data on one machine. The class representing a profile is called
[Profile](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/profiles/profile.h).
In the content layer a profile is called a
[BrowserContext](https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:content/public/browser/browser_context.h;drc=47042255a0d8acfbcf58cb0eea4607ec574b8419;l=108).

Incognito mode is a special kind of profile. The goal of incognito mode is to
allow the user to browse Chrome and use their Chromebook without it remembering
anything about that session. ChromeOS uses a
[OffTheRecordProfile](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/profiles/off_the_record_profile_impl.h?q=OffTheRecordProfile)
object to accomplish this, which provides all the same storage interfaces as a
normal Profile, but doesn’t write that data to disk. When the
OffTheRecordProfile is destroyed, all the state from that session is gone.

Guest mode is an extension of incognito mode for when a user wants to lend their
computer to someone else. In addition to not saving any information about the
session, it also starts the browser in a fresh state. To do this Chrome first
creates a new empty Profile and then immediately creates an OffTheRecordProfile
from it.

### Cryptohomes

User directories are encrypted, and referred to as "cryptohomes", and therefore
users have to provide a key (usually a password) to decrypt and mount it.
When the user signs in and begins their user session, their cryptohome is
mounted/decrypted and available. When the user signs out, the cryptohome is
unmounted/encrypted.

### KeyedServices

[KeyedServices](https://source.chromium.org/chromium/chromium/src/+/main:components/keyed_service/core/keyed_service.h?q=KeyedService&ss=chromium%2Fchromium%2Fsrc)
are separated from the Profile in the codebase in order to ensure the services
are started up and torn down in the correct order. Read more about
[Chromium Profile Architecture](https://www.chromium.org/developers/design-documents/profile-architecture/).
Many features have per-profile data, so it would be a mess if every feature had
to add its own fields to the BrowserContext or Profile objects.

KeyedService also has a two-phase shutdown process. Before the BrowserContext
is destroyed, a Shutdown() method is called on all the KeyedServices associated
with it. Once they have been shut down the second phase destroys the objects.
This is useful when services have dependencies on each other, as the shutdown
phase allows those connections to be broken first. A
[ProfileKeyedServiceFactory](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/profiles/profile_keyed_service_factory.h?q=ProfileKeyedServiceFactory)
creates instances of a service for a Profile and manages dependencies
between services.

Most features use KeyedServices to start up and shut down at the
correct times, if the features want to be tied to the user
session lifetime (which most features do). Some features might want services
that aren't keyed to a profile (e.g., Quick Pair), which are hooked into
the
[shell](https://source.chromium.org/chromium/chromium/src/+/main:ash/shell.h)
[initialization](https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:ash/shell.h;l=848;drc=3b6a047da8255b32ec49f7c5476ed8429118b358;bpv=1;bpt=1)
in ash (the window manager and system UI for ChromeOS).

### Managing accounts

[Account Manager](https://source.chromium.org/chromium/chromium/src/+/main:components/account_manager_core/)
is the source of truth for in-session accounts on ChromeOS. All account
additions, updates, and deletions happen directly on Account Manager, which are
then fanned-out throughout the system via an Observer Design Pattern. Account
Manager is available throughout a User session. It lives outside the concept of
a Chrome "Profile". Conceptually, Profiles track a subset of accounts in
Account Manager.

[Identity Manager](https://source.chromium.org/chromium/chromium/src/+/main:components/signin/public/identity_manager/)
is the "user facing" abstraction for Identity on Chrome. On ChromeOS, it uses
the ChromeOS Account Manager as the source of truth. Identity Manager is tied
to a Chrome "Profile".

### Prefs

Preferences, also known as “prefs”, are key-value pairs stored by Chrome.
[Pref](https://chromium.googlesource.com/chromium/src/+/HEAD/chrome/browser/prefs/README.md)
stores a lightweight value across reboots and optionally tied to a profile.
There are two forms of preferences, profile and local state.

- Profile prefs (also called user prefs) are attached to the user profile.
- Local prefs (also called device prefs) are attached to the local machine,
and therefore apply to all Profiles on the machine. Local prefs are not
synced to the user's Profile.

Each profile has a
[PrefService](https://source.chromium.org/chromium/chromium/src/+/main:components/prefs/pref_service.h)
that manages the profile prefs for the Profile.
A user with different Profiles may have different profile prefs on the
same machine. Read more about user prefs
[here](https://chromium.googlesource.com/chromium/src/+/HEAD/chrome/browser/prefs/README.md).
Profile prefs are saved in the user cryptohome.

Local state prefs are saved outside the user cryptohome (therefore not
encrypted). Read more about the difference between local and profile prefs
[here](http://go/chromium-cookbook-policy-prefs#what-are-preferences).

A user may also have their prefs controlled by policy. Read more about
[Enterprise policy implications on prefs](/chromium-os/developer-library/guides/enterprise/enterprise-policy).

### Sources

- [Anatomy of a Browser 201: The Browser Process presentation](https://docs.google.com/presentation/d/1wM41jcVppQyagSk5ZEbiUvcGwkOnRI5ZK2JpZ9Dm8JA/edit?usp=sharing)
by reillyg@.
- [Profile and Accounts](https://docs.google.com/presentation/d/1f2W1YySPZGsR6LBJZU4KT78URAqbRZ_jILNuMo5uiRs/edit?usp=sharing)
by rsorokin@
