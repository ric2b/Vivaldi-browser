// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

pub mod remote_auth_service;

/// Struct representing the remote device.
pub struct RemoteDevice {
    pub id: i32,
}

/// Trait to be implemented by anything that wants to be notified when remote devices are discovered
/// or lost.
pub trait DeviceDiscoveryListener {
    fn on_discovered(&mut self, remote_device: &RemoteDevice);
    fn on_lost(&mut self, remote_device: &RemoteDevice);
    fn on_timeout(&mut self);
}

/// Trait to be implemented by the struct responsible for scanning for remote devices.
pub trait DiscoveryPublisher<'a, T: DeviceDiscoveryListener> {
    /// Add a listener to the list and return a handle to that listener so it can be removed later.
    fn add_listener(&mut self, listener: &'a mut T) -> i32;
    /// Remove the listener with the given handle.
    fn remove_listener(&mut self, listener_handle: i32);
    /// Notify all listeners that a remote device has been discovered.
    fn device_discovered(&mut self, remote_device: &RemoteDevice);
    /// Notify all listeners that a previously discovered remote device has been lost.
    fn device_lost(&mut self, remote_device: &RemoteDevice);
    /// Notify all listeners that timeout has occurred.
    fn timed_out(&mut self);
}
