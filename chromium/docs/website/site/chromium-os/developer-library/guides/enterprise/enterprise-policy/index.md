---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: enterprise-policy
title: Enterprise policy
---

[TOC]

Here, we extract a few key points from the more general
[add_new_policy.md](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/enterprise/add_new_policy.md).

## Overview

Features can optionally be controlled by enterprise administrators. This
requires a few additions to the code base. For example, consider Nearby Share:

1.  Add an entry to policy_template.json, for example,
    [`NearbyShareAllowed`](https://source.chromium.org/chromium/chromium/src/+/main:components/policy/resources/policy_templates.json;l=17460-17480;drc=02e368ac68c1761743de421113af2eea2b0fdce5).
    Of note, `dynamic_refresh` means policy changes can take effect even while
    the user is logged in.

2.  Add a handler to configuration_policy_handler_list_factory.cc, configuring
    the policy to affect a specific pref. For example, see
    [`NearbyShareAllowed`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/policy/configuration_policy_handler_list_factory.cc;l=1963-1964;drc=0c5ddbb59a6ca8c6eef82927fa6dd7416947481f)
    (but beware its unorthodox handler).

3.  Use
    [`PrefService::IsManagedPreference()`](https://source.chromium.org/chromium/chromium/src/+/main:components/prefs/pref_service.h;l=207-209;drc=ce1e29491ca2e39f8816db88d5a110590cb73a20)
    to determine if a feature is being controlled by enterprise policy. For
    example, we adjust the UI if
    [Nearby Share is disallowed by policy](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/nearby_sharing/nearby_share_settings.cc;l=86-89;drc=f7b4f99c25ddbbb5d4f7edd1d5390fd04851e207).

## Testing enterprise policy

To test an enterprise policy on ChromeOS quickly--without needing to set up a
policy test server (see
[add_new_policy.md](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/enterprise/add_new_policy.md))--
we can simply add a file to the DUT (see also
[linux-quick-start](https://www.chromium.org/administrators/linux-quick-start)).
For example, to set the `NearbyShareAllowed` policy value,

1.  ssh into your DUT,
2.  create the file `/etc/opt/chrome/policies/managed/test_policy.json`[^1],
3.  add `{"NearbyShareAllowed": true}`,
4.  wait for the policy to refresh or click "Reload policies" at
    `chrome://policy`,
5.  confirm the correct policy value at `chrome://policy` on the DUT.

No special account is required on the DUT to perform this testing.

[^1]: For unofficial Chromium builds, this might need to be
    `/etc/chromium/policies/managed/test_policy.json`. See
    [linux-quick-start](https://www.chromium.org/administrators/linux-quick-start).

## Using the Enterprise Policies Admin Panel

This section describes the steps for setting a policy rule from the
[Admin panel](https://admin.google.com/), and seeing it applied on your managed
device/account. *Note that you can only login to the Admin panel with a
@managedchrome.com user account, otherwise you will be stuck in the select
accounts page.*

### Obtain a @managedchrome account

If you haven't already, obtain a @managedchrome account with "Super Admin"
privileges by filling out the form at go/managed-chrome-account.

Alternatively, you can do this by asking someone on the team who does have a
@managedchrome account with "Super Admin" privileges to:

1.  Login to the [Admin panel](https://admin.google.com/) with their
    @managedchrome account.
2.  Add a new user (this can be done in the Home page and expanding the "Users"
    card > "Add a user").
3.  Updating the newly created account with "Super Admin" privileges by
    searching for the username in the search box and clicking into the subpage,
    and then going to the "Admin roles and privileges" section.

### Create an Organizational unit

An organizational unit should be created that your @managedchrome account will
be within. Any policy updates you make to the organizational unit in the admin
panel will be applied to accounts/devices within that unit.

Prior to creating a new unit, the account will default to the root parent
managedchrome.com unit. Your new organizational unit will be a descendent of the
managedchrome.com unit.

To create an organizational unit:

1.  Log into to the [Admin panel](https://admin.google.com/) with your
    @managedchrome account.
2.  Go to Directory > Organization units and add new organizational unit.
3.  Go to Directory > Users and navigate to your @managedchrome.com account.
4.  Click "Change Organizational Unit" and select the new unit that was created.

### Setting an enterprise policy rule

If you want to make device policy changes, you need to go through OOBE on your
test device and enterprise enroll your test device with the @managedchrome
account. Then, ensure that you are logged in with your @managedchrome account
and not guest.

After you have completed the previous steps, you are now ready to set policies
via the admin panel and see the policies applied to your device/account. Ensure
that whenever you apply a policy in the admin panel, you do so to the
organizational unit your @managedchrome account is associated with, otherwise
your device will see no change.

#### Example

The following example runs through how to add a new policied Wi-Fi network that
your managed chromebook can connect to without entering a passphrase on your
chromebook. Policied Wi-Fi networks will display a building icon in its row in
the list of available WiFi networks shown at the WiFi subpage of OS Settings on
your managed on your device.

1.  Disconnect from the Wi-Fi network your test device is currently connected
    to, and forget it so that if the next time you try to connect to the Wi-Fi,
    you would have to re-enter the passphrase.
2.  Login to the [Admin panel](https://admin.google.com/) with your
    @managedchrome account on a device other than your test device.
3.  Go to Devices > Networks > Wi-Fi
4.  Ensure that the "Organizational unit" is the organizational unit you
    created.
5.  Click "Add WiFi", at which point you'll be taken to a form to add a Wifi.
6.  In the "Platform access" section, make the selections that fit your use case
    (likely "Chromebooks (by user)" and "Chromebooks (by device)".
7.  Enter a Wi-Fi Name and SSID. Note that the "SSID" is likely the name of the
    Wi-Fi that is shown to your test device.
8.  If your Wi-Fi requires a passphrase, you will likely have to change the
    "Security settings" in the "Details" section of the form. For WiFi at your
    home, for example, you may likely select a "Security Type" of "WPA/WPA2".
    Ensure you add the passphrase to the Wi-Fi in the form.
9.  Make any other desired changes.
10. Click save.
11. Log out of your chromebook and log back in.
12. Go to OS Settings and the Wi-Fi network subpage, and you should be able to
    see the policied Wi-Fi network (if it is in range) with a building icon in
    the row.
13. Try connecting, and you'll notice that you will be able to connect without
    having to enter a passphrase.

## Building a local instance of the Network Admin Dpanel for development

This section describes how you can spin up a local instance of the Admin DPanel
for Network related policy development. More details of this process can be
found
[here](https://g3doc.corp.google.com/chrome/cros/dpanel/g3doc/development/getting_started.md).

1.  Join mdb/dpanel-local-dev-policy and mdb/chrome-enterprise-debug-access. You
    must be part of these groups for anything to work. It may take a day or two
    to be approved.
2.  Follow the instructions
    [here](http://go/dpanel-test-accounts#test-gaia-new-domain) to create a new
    domain. In the process of following those instructions, an admin account for
    the domain will be created. You can think of the domain as the organization
    that an admin will apply policies to.
3.  Add the domain to the allow list shown
    [here](http://google3/googledata/experiments/chrome/cros/properties/domains/internal.gcl;l=57;rcl=504012747).
    It may take a day or two for the allow list to be updated.
4.  Create a new g4d workspace.
5.  Run the following command:

    ```shell
    boq run --node=java/com/google/chrome/cros/boq/dpanel/data/networks/service,java/com/google/chrome/cros/boq/dpanel/ui
    ```

    Note that there should not be any spaces separating the nodes, only commas.
    The command doesn't work if there are spaces.

    Also note that there is also a script to start nodes that may be easier to
    use. go/dpanel-getting-started#start-script The script will start bequt by
    default.

6.  Visit the following pages (preferably incognito mode), noting that if you
    are running from a cloudtop, replace `localhost.corp.google.com` with your
    cloudtop's hostname:

    *   DpanelChromeUiServer for networks:
        http://localhost.corp.google.com:9879/ac/networks
        *   You should login to the dpanel with the admin account you created as
            part of step 1.
        *   Make sure to use the full URL listed above, as you may get an error
            404 if trying to access just the root '/'!
    *   Backend Query Tool (BEQuT): http://localhost.corp.google.com:9880/
    *   Hexa web console: http://localhost.corp.google.com:9110/

    Note that any UI changes you make should reflect automatically without
    having to re-reun the command, but sometimes you may have to re-run the
    command if the localhost gets in a bad state.

    If using a ChromeOS device to access the local server, use incognito mode
    and make sure to enable Beyond Corp extension in incognito mode.

## Dpanel RPC studio instructions

Note that the following instructions assumes you've completed the
[steps](/chromium-os/developer-library/guides/enterprise/enterprise-policy#building-a-local-instance-of-the-network-admin-dpanel-for-development)
to build a local instance of the Network Admin Dpanel. This section describes
how you can verify fetching and updating protos (new or existing) by sending
traffic to the Dpanel data node.

1.  You need an owned test account. Go to go/dev-test-account-conversion. Fill
    out the form
    [like so](https://screenshot.googleplex.com/99kwd2EGu8C6KN2.png) using the
    admin account of the domain you created previously in the "Accounts" field.
2.  Start dpanel locally with that boq run command
    go/dpanel-getting-started#running-locally.
3.  Start rpc studios go/local-rpcstudio. Navigate to the url that is printed to
    the console.
4.  You should see your local networks service in
    [the list of targets](https://screenshot.googleplex.com/A4hSPQx4suDMsqo.png).
5.  [Select](https://screenshot.googleplex.com/8Be3nv8FKkAHEfY) whatever method
    you want - e.g. GetNetworkPolicy or UpdateNetworkPolicy
6.  Go to end user credentials. Turn on advanced mode. Select Test Gaia OTA.
    Should look like
    [this](https://screenshot.googleplex.com/6YuKyh5TY4DgS8J.png).
7.  Craft the request. It is probably easiest to start from a real network
    request and edit from there. Here is how you can get details of a real
    [UpdateNetworkPolicy](https://screenshot.googleplex.com/5qSTXd8xLN5qRXA)
    request from [boq-studio/](https://boq-studio.corp.google.com/)

    1.  Go to your local dpanel instance, and save a network.
    2.  Go to [boq-studio/](https://boq-studio.corp.google.com/). You should see
        your
        [local instance](https://screenshot.googleplex.com/ByLpduBLC9YRfyn.png).
        Click on it.
    3.  [Search the outgoing requests](https://screenshot.googleplex.com/4SsuCGnmXDPG5pL.png).
        Click on the outgoing request you want to copy. And
        [copy the request](https://screenshot.googleplex.com/6rTQDSMSNJNirJi.png).
    4.  Go back to rpc studio.
        [Select the text proto tab, and paste the request](https://screenshot.googleplex.com/BVvFc3cR85mAhxo.png).
    5.  Select the editor tab, and make your modifications.

8.  You should be ready to send the rpc now.

9.  You can see the request and response in rpc studio. If you want to debug the
    code. You can do so with ciderd go/cider-user-guide/debugging. See the
    section about attaching to a running process
    go/cider-user-guide/debugging#attaching-to-running-processes to attach to
    your local dpanel instance. This will allow you to trigger breakpoints in
    cider when you send your rpcs from rpc studio.

## Log regeneration after proto changes

After modifying DPanel protos (e.g this
[CL](https://critique.corp.google.com/cl/440438503), you may have to regenerate
test logs in order to submit the CL. Otherwise, many of the tests will fail.
Make sure to be in the cider workspace where your proto changes are, and use one
of following methods to regnerate the logs:

### Method 1

```
/google/data/ro/teams/frameworks-test-team/rpcreplay-cli/live/rpcreplay regen \
//javatests/com/google/ccc/hosted/devicemanagement/emm/adminconsole/policy/service/integration:all
```

*   Automatically adds all the generated logs into the workspace
*   You need to be in one of
    [these groups](http://google3/java/com/google/ccc/hosted/devicemanagement/emm/adminconsole/gem-ui.blueprint;l=383-392;rcl=498038236).

### Method 2

```
guitar run --cluster=gem-ui-integration -w=//javatests/com/google/ccc/hosted/devicemanagement/emm/adminconsole/policy/service:guitar_borg_workflow  --version=citc:LOCAL
```

After the above command completes, a "sponge_invocation_id" should be generated.
Use it in the following command:

```
/google/bin/releases/dasher-engprod/tools/regenerate_rpc_logs/regenerate_rpc_logs \
--sponge_invocation_id=<sponge_invocation_id>
```

### Method 3

```
blaze test --notest_loasd --test_strategy=local \
javatests/com/google/ccc/hosted/devicemanagement/emm/adminconsole/policy/service/integration:EmmAdminConsolePolicyServiceTest_RpcRecord
```

After the above command completes, a "sponge_invocation_id" should be generated.
Use it in the following command:

```
/google/bin/releases/dasher-engprod/tools/regenerate_rpc_logs/regenerate_rpc_logs \
--sponge_invocation_id=<sponge_invocation_id>
```

*   This method is unlikely to work unless the local computer you work on is
    powerful.
*   You may encounter blaze resoucre issues otherwise.

### Notes

*   A regen runs every few hours on head
*   If your CL has the same failing tests as the ones on head, reach out to the
    group that is on call to fix the tests (e.g oncall/access-security-desktop):

    *   They may have to fix these tests before the proto change can land
    *   You may have to force submit the CL.

*   See more detailed notes on log regeneration
    [here](https://g3doc.corp.google.com/java/com/google/ccc/hosted/devicemanagement/emm/adminconsole/policy/service/testing/g3doc/IntegrationTests.md#record-regenerate-the-logs).

## Debugging RPC calls on DPanel

In DPanel, you can use [Sherlog](https://g3doc.corp.google.com/repository/pipeline_diagnostics/sherlog/g3doc/index.md)
a cross-stack event tracing service, to find out what is being passed in an RPC
call. It collects data for debugging functional defects.
