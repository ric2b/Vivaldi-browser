---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Developer Library > Guides
page_name: cv3-apis
title: Cv3 ChameleonD APIs
---

# Cv3 ChameleonD APIs

### Reset(self):
Disconnect all ports, remove all EDIDs, discard all captured frames. Basically return Chameleon to the known fresh state.

### GetChameleondLogs(self, cursor="main"):
Get logs from journalctl. Useful for debugging. For tests not so much.

### GetVersions(self):
Gets versions of various components. Useful for debugging and logging. Tests shouldn’t act differently based on versions.

### GetDetectedStatus(self):
Gets list of all ports and if there is physically something connected to the port. Note some ports share physical connection, so all of them will have the same detect status. Note2 only parent MST port (MST Stream 0) is on the list, other MST ports are omitted.

### GetSupportedPorts(self):
Get list of all ports IDs. Note only parent MST port (MST Stream 0) is on the list, other MST ports are omitted.

### GetSupportedInputs(self):
At the moment there are no outputs so this is exactly the same as GetSupportedPorts.

### GetSupportedOutputs(self):
At the moment there are no outputs so the list is empty.

### IsPhysicalPlugged(self, port_id):
Check if there is physically something connected to the connector used by the port.

### ProbePorts(self):
Get list of all ports that are physically connected to something. Note some ports share physical connection, so there can be more probed ports than cables plugged into chameleon. Note2 only parent MST port (MST Stream 0) can be on the list, other MST ports are omitted.

### ProbeInputs(self):
At the moment there are no outputs so this is exactly the same as ProbePorts.

### ProbeOutputs(self):
At the moment there are no outputs so the list is empty.

### GetConnectorType(self, port_id):
Connector type name of the port (e.g. “HDMI”, “DP”)

### IsConflict(self, port_id_a, port_id_b):
Check if two ports are in conflict i.e. if the can or cannot be plugged at the same time.

### GetMutuallyExclusivePorts(self, port_id):
Gets list of all ports that cannot be plugged at the same time as the provided port.

### GetPortName(self, port_id):
Gets a human friendly name of the port. Shouldn’t be used by tests logic but can be useful for logging.

### GetChildren(self, port_id):
Gets a list of children ports of the provided port. Children ports are similar to ports but cannot be used with Plug, Unplug and all HPD methods. Instead the PlugWithChildren function can be used to plug a child port. Unplugging the parent port will unplug the child too.

### IsMst(self, port_id):
Check if port is part of the MST chain. It can be either a child or a parent.

### HasAudioSupport(self, port_id):
Check if the port has audio. None port has that at the moment.

### HasVideoSupport(self, port_id):
Check if the port has video.

### WaitVideoInputStable(self, port_id, timeout=None):
Wait until there is a stable video stream received on the port.

### CreateEdid(self, edid):
Create EDID from given bytes. Returns the ID of the EDID that can be used in other EDID methods.

### DestroyEdid(self, edid_id):
Removes EDID from internal database. The ID shouldn’t be used anymore.

### ReadEdid(self, port_id):
Read EDID that will be/was used at the moment of plugging the port.

### ApplyEdid(self, port_id, edid_id):
Set EDID for the port. The EDID will be used on plugging the port.

### IsPlugged(self, port_id):
Check if the port is plugged and is emitting HPD. Note that a port without cable in the physical connector can be still plugged at the same time.

### Plug(self, port_id):
Plug the port. Start emitting HPD. EDID has to be set first to the port. Note plugging MST port using this function will result in plugging just one stream.

### PlugWithChildren(self, port_id, children_ids):
Plug the port and its children specified on the list.

### Unplug(self, port_id):
Unplug the port i.e. HPD is no longer emitted, EDID is cleared. Children of the port are also unplugged.

### UnplugHPD(self, port_id):
Deassert HPD without clearing EDID.
FireHpdPulse(...):
Performs multiple HPD unplugs and plugs.

### FireMixedHpdPulses(self, port_id, widths_msec):
Performs multiple HPD pulses with specified duration.

### ScheduleHpdToggle(self, port_id, delay_ms, rising_edge):
Sets HPD to the opposite value to rising_edge and after specified delay sets HPD to the value of rising_edge. After setting the initial HPD value, the function returns, so the user may do something else.

### DumpPixels(self, port_id, x=None, y=None, width=None, height=None):
Get one frame from the specified port. Note that cropping is not supported. Note2 it will clear previously captured frames.
CaptureVideo(...):
Capture specified number of frames from the specified port. Note that cropping is not supported. Note2 it will clear previously captured frames.

### GetCapturedResolution(self):
Get resolution of the last captured frame.

### ReadCapturedFrame(self, frame_index):
Get binary data of the specified frame that was collected after the last CaptureVideo call.

### DetectResolution(self, port_id):
Get resolution of the video stream that is received at the moment.

### GetVideoParams(self, port_id):
Get timings of the video stream that is received at the moment.

### GetMacAddress(self):
Get MAC address of the chameleon.

### GetCapturedFrameMetadata(self, frame_index):
Get metadata of the specified frame that was collected after the last CaptureVideo call. Metadata includes timestamp generated by the kernel upon receiving the frame.

## Limitations

* No audio support - can be fixed by SW update
* No support for video output
* Can detect missed frames, but not number of missed frames - not possible with current FPGA bitstream
* Can plug multiple ports, but frames can be actively collected only from one port at the same time - this can be fixed by SW update (in limited way, depending on selected ports, HW may be not capable to save frames to the memory from multiple streams e.g. two 4K@60Hz streams cannot be collected simultaneously)
* No cropping - can be fixed by SW update
* Limited space for caching frames (768 MiB). This translates to aprox. 40 frames of 4K stream.
* When there is no stream on the port, the resolution of 1920x1080 is reported. After losing the stream, the last one is reported as the current one.
* Only supported pixel format is RGB24
