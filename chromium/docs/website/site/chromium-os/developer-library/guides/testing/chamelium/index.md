---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Developer Library > Guides
page_name: igt
title: Getting Started with Chamelium V3
---

🎉 Congratulations on receiving your Chamelium V3 (Cv3) device.🎊
If you made it here, you're one of the lucky ones who received the all-new Cv3 device.🥳

Please follow the following steps to successfully setup your Cv3 experience:

[TOC]

## 1. Check Your Package
Every Chamelium package should include the Cv3 board (yellow) and a power adapter.

## 2. Join the Chamelium Group
Head to [Chameleon-v3-users-external](https://groups.google.com/a/google.com/g/chameleon-v3-users-external) and request to join the group.
This is where you'll find relevant discussions about Chamelium, and where you can ask any questions about it and someone familiar with the topic would be able to help out.
**New Releases** will also be announced on this group. Releases are sent out regularly with the latest firmware and daemon updates. [Example of the latest release email.](https://groups.google.com/a/google.com/g/chameleon-v3-users-external/c/CsP4D5h5QKE)

## 3. Setup Your New Shiny Device
To setup your Cv3, head to [Chameleon v3 Setup, Deployment, and Testing](https://chromium.googlesource.com/chromiumos/platform/chameleon/+/refs/heads/main/v3).
The doc should include everything you need to know to setup your device with the latest release.

## 4. Test Out Cv3 with Your DUT
Before you run any tests, let's make sure Cv3 runs well and is reachable. We will interact manually with Cv3 to validate everything
### a. Grab Cv3's IP
Cv3 advertises its IP on boot. If you don't know the IP, reset Cv3's power and follow steps 4-5 in the [setup doc: Boot the Linux System](https://chromium.googlesource.com/chromiumos/platform/chameleon/+/refs/heads/main/v3#boot-the-linux-system:~:text=IP%20address(es)%3B%20make%20note%20of%20these.)
I recommend you save it in your `~/.ssh/config` for posterity
```
Host cv3
  HostName 123.45.678.910 # the IP you grabbed above
  User cros
  StrictHostKeyChecking no
  VerifyHostKeyDNS no
  CheckHostIP no
  UserKnownHostsFile /dev/null
  IdentitiesOnly yes
  IdentityFile %d/.ssh/testing_rsa # https://chromium.googlesource.com/chromiumos/chromite/+/master/ssh_keys/testing_rsa # nocheck
```

### b. Understand the Ports
While the info looks a bit daunting as you're about to get started, the [Single Port](#single-port) subsection below is important to get your validation in place.
**The Hardware**
Cv3 has 4 physical ports to physically plug
```
+--------+---------+
|  DP 1  |  DP 2   |
+--------+---------+
| HDMI 1 | HDMI 2  |
+--------+---------+
```
**Single Port**
If you're using the most simple case, you can only active in 1 at a time as the ITE ship can only drive 1 port.
You can physically connected as many as you want, but you can only tell Cv3 to "active" (`Plug()` in Cv3 lingo) 1 ITE port.
```
[PORT NAME] = PORT ID
ITE_DP1 = 0
ITE_DP2 = 1
ITE_HDMI1 = 2
ITE_HDMI2 = 3
...
```
**Multiple Ports**
If you require more than 1 physically plugged monitors to be active at the same time, Cv3 can support more ports, but we will have to use different port IDs.
```
...
ITE_HDMI1 = 2
ITE_HDMI2 = 3
FPGA_DP1 = 4
FPGA_DP2 = 5
...
```
The ITE ship can still support only 1 display, so you can only get 1 HDMI, but the FPGA chip can support multiple physical displays, so the most you can do here is 3 simultaneous ports: (`ITE_HDMI1|2`), `FPGA_DP1` and `FPGA_DP2`.
*NOTE: Physically speaking, `ITE_DP1`&`FPGA_DP1` relate to the same physical port (same for \*_DP2), but the port ID is what tells Chameleon whether it's able to drive multiple displays and reroutes the correct hardware.*
**MST**
The discussion is long, so jump to [Understand MST](#n-understand-mst) section below

### c. Test Out the Screenshot Tool
* Connect your DUT to Cv3. Any port is fine but make note of the Cv3 physical port and port ID you're connected to.
* Your DUT may or may not recognize Cv3, depending on the port plug status. If it recognizes it as an external monitor, that's great news. If it doesn't, don't worry about it, we're here to test it.
* Run the Screenshot Tool as described in the [setup doc: Interact with chameleond directly](https://chromium.googlesource.com/chromiumos/platform/chameleon/+/refs/heads/main/v3#interact-with-chameleond-directly:~:text=./v3/clients/-,screenshot,-%2D%2Dchameleon_host%20your_chameleon_ip%20%2D%2Dport_id)
* If it succeeds and you get a screenshot of your external display (the photo result will be saved in the directory where you called `screenshot`), then your Cv3 is reachable by the DUT and the ports are functional.
* If that doesn't work, let's interactively play with it and see if it's reachable.

### d. Interactively Play with Cv3
0. If you're on corp network, follow the same workaround [here](https://chromium.googlesource.com/chromiumos/platform/chameleon/+/refs/heads/main/v3#interact-with-chameleond-directly) `ssh cv3 -L 9992:localhost:9992`
1. In your chromeos repo, navigate to `cd src/platform/chameleon`
2. call `python client/test_server.py --chameleon_host cv3` (on corp, this would be `python client/test_server.py --chameleon_host localhost`)
3. From the interactive shell, plug the display you want `>>> p.Plug(0)`
> If you can't do that, then your Cv3 is unreachable. Make sure it's on and you can ssh to it

*NOTE: This interactive shell can be run from within Chamelium itself*
1. `ssh cv3`
2. `python chameleon/client/test_server.py --chameleon_host localhost`
3. `>>> p.Plug(0)`

All calls are found in the [interface file](https://chromium.googlesource.com/chromiumos/platform/chameleon/+/refs/heads/main/chameleond/interface.py)

## 5. Understand MST
With V3 that now supports MST, things get a bit more fancy and ports are a bit more dynamic than described earlier. Here is explanation for the current state:
```
[PORT NAME] = PORT ID
ITE_DP1 = 0
ITE_DP2 = 1
ITE_HDMI1 = 2
ITE_HDMI2 = 3
FPGA_DP1 = 4
FPGA_DP2 = 5
FPGA_DP2_MST0 = 5
FPGA_DP2_MST1 = 6
FPGA_DP2_MST2 = 7
FPGA_DP2_MST3 = 8
```
*NOTE: For tests, while the ports are constant right now, it is better to use other properties (such as GetConnectorType()) or iterate over ports instead of hardcoding these numbers.*
**Port Conflict and Discoverability**
```
def IsConflict(self, port_id_a, port_id_b)
def GetMutuallyExclusivePorts(self, port_id)
```
`IsConflict()` allows to discover if two particular ports can be plugged at the same time and `GetMutuallyExclusivePorts` discovers all ports that will be in conflict. Tests should check for this before plugging multiple ports at the same time.
Assume portA is in conflict with portB and portA is plugged. When you try to plug portB, Chameleond will raise an exception. portA should be explicitly unplugged first so plugging portB will succeed.
```
def GetPortName(self, port_id)
```
This is an API for interactive users to get human friendly name e.g. `FPGA DP1 (left)` or `ITE DP2 (right)`
**The MST Port**
```
def IsMst(self, port_id)
```
MST ports are NOT discoverable by `GetSupportedPorts`. Only `FPGA_DP2` can be discovered because it also functions as a standalone port. `IsMst` is introduced to test if port is capable of MST. Calling it for `IsMst(FPGA_DP2)` will `return true`.
To find out how many extra ports can be configured behind the MST port `GetChildren` can be used. It will return a tuple of children ports *(not discoverable by other APIs)*.
**The MST Children Ports**
```
def GetChildren(self, port_id)
def PlugWithChildren(self, port_id, children_ids)
```
Calling `GetChildren(FPGA_DP2)` return 3 Port IDS: `FPGA_DP2_MST1`, `FPGA_DP2_MST2`, `FPGA_DP2_MST3` so in total there is up to 4 monitors on this port.
Children ports can be used with all APIs except `Plug()` and `Unplug()`.
To Plug them, call `PlugWithChildren`.
For example, if you want to plug `MST0`, `MST2` and `MST3` displays, then do like the following Python Example:
```
# Example of discovering required ports
mst_port = None
for port in chameleond.GetSupportedPorts():
  if chameleond.IsMst(port):
    mst_port = port
    break
children = chameleond.GetChildren(mst_port)
# Get last two ports
children = children[-2:]

# Assume there is EDID with id X already created
chameleond.ApplyEdid(mst_port, X)
for child in children:
  chameleond.ApplyEdid(child, X)

# Finally plug all three monitors
chameleond.PlugWithChildren(mst_port, children)
```
Note: *The first MST port `MST0` (parent port) will always be used if MST is used, as it is the port controlling the HPD line.
Using the standard `Plug()` on the MST parent will plug just this port without any children. Using `PlugWithChildren` on any port with empty children list (i.e. `ITE_HDMI1`) will have the same effect as `Plug` on that port.*

**Unplug**
```
def Unplug(mst_port)
```
To unplug all MST ports just use regular `Unplug`.
