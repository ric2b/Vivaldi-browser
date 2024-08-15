---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: policy-prefs
title: Preferences and policies
---

[TOC]

## Preferences
### What are Preferences?

A Preference is a mechanism to store values across reboots and
optionally sync to a profile. We typically use Preferences if we want to
store a user selected setting and preserve that across system reboots. Note
that these are not the same as a C++ global/singleton variable as those are
tied to the lifetime of the program (i.e. Chrome).

There are two forms of preferences, **profile** and **local state**.

Profile Preferences - The value is attached to a specific
[user profile](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/profiles/profile.h;l=69;drc=adb945c5a0060e6024cb174f6027d13d7ff03058)
and can be optionally synced. These values are only applied and changed by the
current signed in profile. If the preference is synced, it will be available in
any device the User signs into. If the profile preference is not synced it stays
local to the device.

Local State Preferences - The value is attached to the machine and is not
synced to a profile. Since these are not tied to a profile, Local State
Preferences are applied to all session states of the device, such as the
login screen, and are applied to all signed in profiles.

Tip: If there is a device
setting that you only want the device owner (i.e. first user to be added to
the device) to change, consider using a [CrosSetting](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/settings/cros_settings.h;drc=1eaedc46d049302c1b9b8b948e696660fd2c8395).
More details on the ownership model [here](https://docs.google.com/document/d/1iHk2Nht9h9rsUiBgtG62ERONLh-vPgoQxQQyBuVIPB4/edit#heading=h.fkznh2f0zxp1).

### Which Preference should I use?

When adding a Preference check on what are the parameters for the value
you're storing.

If the value is useful only for a signed in user and makes
sense to be synced across different devices (e.g. Wallpaper),
then a Profile Preference is probably ideal. If the value is only
useful for the current session (e.g. tracking if the user has recently
did an action), then it should probably not be a Preference.

If you have a value that you want to preserve across reboots and available in
all session states, then a Local State Preference is preferred. Keep in mind
that this preference will also be available for all profiles that sign into
the device, so please do not store any profile-tied values to a local state
pref (some exceptions may apply such as parent-child accounts).

Bottom line is that if there's a value that you want to store and keep across
system reboots then Preferences are your best bet, but be sure to pick the
correct Preference for your use case.

### Preference Implementation

Tip: The following code examples are specific for Ash. Browser based prefs
can be seen [here](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/prefs/browser_prefs.h;drc=31a359c5cfa01f98a65cdac507b5c582fe539581).
Further documentation on prefs can be found [here](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/prefs/README.md;drc=40b5dc0d191f0262c9aa3e0a82ef30e12a0d19df).

#### Step 1: Define the preference name

* Add the preference constant variable in [ash_pref_names](https://source.chromium.org/chromium/chromium/src/+/main:ash/constants/ash_pref_names.h;drc=02b9357c35d4dc6d660d63816498989fb720487e)

#### Step 2: Register the Preference

*   All Preferences need to be registered by a [PrefRegistry](https://source.chromium.org/chromium/chromium/src/+/main:components/prefs/pref_registry.h;drc=8ba1bad80dc22235693a0dd41fe55c0fd2dbdabd)
*   Prefer the Preference to be registered as close as possible to the code that
uses it.
* If your preference needs to be synced, you will need to add the
`SYNCABLE_OS_PREF` parameter. [Source](https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:components/pref_registry/pref_registry_syncable.h;l=60;drc=9ba7e889bc57846a89add93a003e4443dfbeb736)

```
// Static:
void AshAcceleratorConfiguration::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  // Local profile pref.
  registry->RegisterDictionaryPref(prefs::kAcceleratorOverrides);

  // Synced profile pref.
  registry->RegisterBooleanPref(
      prefs::kAccessibilityAutoclickStabilizePosition, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF)
}
```

* Register the Preference in [ash_prefs.cc](https://source.chromium.org/chromium/chromium/src/+/main:ash/ash_prefs.cc;drc=519d91cf2c127b10de48542136aa43c93953f07f)

```
void RegisterProfilePrefs(PrefRegistrySimple* registry, bool for_test) {
  ...
AshAcceleratorConfiguration::RegisterProfilePrefs(registry);
  ...
}
```

#### Step 3: Set and Retrieve the Preference
Tip: Preferences store [base::Value](https://source.chromium.org/chromium/chromium/src/+/main:base/values.h;drc=eb544e2a5ccf5bbf8ee245e70db066fd3a1cef7f)
types, which includes your primitive types but also data structures such as
Dictionaries and List.

##### Retrieving Preference values

* You will need access to a [PrefService](https://source.chromium.org/chromium/chromium/src/+/main:components/prefs/pref_service.h;drc=2eb35a15c88019f10165be3d5f4b579fe2a194f2)
in order to interact with Preferences.

```
// Since this is a Profile pref, we need the active signed in user ID.
AccountId GetActiveAccountId() {
  return ash::Shell::Get()->session_controller()->GetActiveAccountId();
}

// Get the associated PrefService for the current signed in account.
PrefService* GetUserPrefService(const AccountId& account_id) {
  return ash::Shell::Get()->session_controller()->GetUserPrefServiceForUser(
      account_id);
}

// Fetch the pref value.
auto* pref_service = GetUserPrefService(GetActiveAccountId());
const base::Value::Dict& override_prefs =
    pref_service->GetDict(prefs::kAcceleratorOverrides);
```

##### Setting Preference values

* Recall that Preference values are typed as base::Value's.

```
// Helper function to convert a std::map<T,T> to base::Value::Dict<T,T>
base::Value::Dict CreateAcceleratorOverrides() { ... }

auto* pref_service = GetUserPrefService(GetActiveAccountId());
pref_service->SetDict(prefs::kAcceleratorOverrides,
                      CreateAcceleratorOverrideDict());
```

## Policy

## What is a Policy?
A Policy is a configuration rule set that an administrator can enforce to
their managed devices.
Policies are typically used in either a enterprise or education setting.

In ChromeOS, a device can be managed by an administrator
so that they can set specific settings that the user may or may not be able to
modify. An infamous example is that there is a policy to disable the Chrome
Dinosaur game, which we see used for Chromebooks in education.

## Using Policies
Before implementing a policy, please ensure that a policy is required. Also please keep in mind that policies will be reviewed by the enterprise/edu team.

Policies are typically backed by a Preference. A Policy itself will not do
anything until you attach it to a Preference.

There are existing documentation on implementing a Policy [here](https://source.chromium.org/chromium/chromium/src/+/main:docs/enterprise/add_new_policy.md;drc=7c5b6a05ac70c5437ae0da083744a94a78e715a8).

Example of adding a simple policy [here](https://chromium-review.googlesource.com/c/chromium/src/+/2195757).

## Additional tips:

* If a preference can be toggled in the UI and can also be configured by
a policy, you should always check the state of the pref in the UI.
 * Example:

 ```
  // TypeScript WebUI:
  // Check if a pref in OSSettings that is used in the UI is enforced
  // (i.e. policy configured).
  private isDefaultSearchEngineEnforced_(
      pref: chrome.settingsPrivate.PrefObject): boolean {
    return pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED;
  };
 ```

* It is encouraged that you design your Policy to be dynamic so that when an
 admin changes the value the user does not require a restart. You can achieve
 this by observing policy changes via [`PrefChangeRegistrar`](https://source.chromium.org/chromium/chromium/src/+/main:components/prefs/pref_change_registrar.h;drc=4d4e24099b3bdced741623fd4e96aaf444809994).
