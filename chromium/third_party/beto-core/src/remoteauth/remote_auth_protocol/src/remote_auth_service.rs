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

use crate::{DeviceDiscoveryListener, DiscoveryPublisher, RemoteDevice};

pub struct RemoteAuthService<'a, T: DeviceDiscoveryListener> {
    listeners: Vec<&'a mut T>,
}

impl<'a, T: DeviceDiscoveryListener + PartialEq> RemoteAuthService<'a, T> {
    pub fn new() -> RemoteAuthService<'a, T> {
        RemoteAuthService {
            listeners: Vec::new(),
        }
    }

    pub fn is_remote_auth_supported() -> bool {
        true
    }
}

impl<'a, T: DeviceDiscoveryListener + PartialEq> DiscoveryPublisher<'a, T>
    for RemoteAuthService<'a, T>
{
    fn add_listener(&mut self, listener: &'a mut T) -> i32 {
        self.listeners.push(listener);
        (self.listeners.len() - 1) as i32
    }

    fn remove_listener(&mut self, listener_id: i32) {
        let listener_index = listener_id as usize;
        if self.listeners.len() > listener_index {
            self.listeners.remove(listener_index);
        }
    }

    fn device_discovered(&mut self, remote_device: &RemoteDevice) {
        for listener in self.listeners.iter_mut() {
            listener.on_discovered(remote_device);
        }
    }

    fn device_lost(&mut self, remote_device: &RemoteDevice) {
        for listener in self.listeners.iter_mut() {
            listener.on_lost(remote_device);
        }
    }

    fn timed_out(&mut self) {
        for listener in self.listeners.iter_mut() {
            listener.on_timeout();
        }
    }
}

impl<'a, T: DeviceDiscoveryListener + PartialEq> Default for RemoteAuthService<'a, T> {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(PartialEq)]
    struct TestListener {
        pub remote_ids: Vec<i32>,
        pub got_timeout: bool,
    }

    impl TestListener {
        pub fn new() -> TestListener {
            TestListener {
                remote_ids: vec![],
                got_timeout: false,
            }
        }
    }

    impl DeviceDiscoveryListener for TestListener {
        fn on_discovered(&mut self, remote_device: &RemoteDevice) {
            self.remote_ids.push(remote_device.id);
        }

        fn on_lost(&mut self, remote_device: &RemoteDevice) {
            if let Some(index) = self.remote_ids.iter().position(|i| *i == remote_device.id) {
                self.remote_ids.remove(index);
            }
        }

        fn on_timeout(&mut self) {
            self.got_timeout = true;
        }
    }

    #[test]
    fn single_listener_discover_notification() {
        let mut listener = TestListener::new();
        let mut ras = RemoteAuthService::new();
        let remote_device = RemoteDevice { id: 42 };

        ras.add_listener(&mut listener);
        ras.device_discovered(&remote_device);

        assert_eq!(listener.remote_ids.len(), 1);
        assert_eq!(listener.remote_ids[0], 42);
    }

    #[test]
    fn all_listeners_discover_notification() {
        let mut listener1 = TestListener::new();
        let mut listener2 = TestListener::new();
        let mut ras = RemoteAuthService::new();
        let remote_device = RemoteDevice { id: 42 };

        ras.add_listener(&mut listener1);
        ras.add_listener(&mut listener2);
        ras.device_discovered(&remote_device);

        assert_eq!(listener1.remote_ids.len(), 1);
        assert_eq!(listener2.remote_ids.len(), 1);
        assert_eq!(listener1.remote_ids[0], 42);
        assert_eq!(listener2.remote_ids[0], 42);
    }

    #[test]
    fn listeners_discover_notification_for_all_devices() {
        let mut listener = TestListener::new();
        let mut ras = RemoteAuthService::new();
        let remote_device1 = RemoteDevice { id: 42 };
        let remote_device2 = RemoteDevice { id: 13 };

        ras.add_listener(&mut listener);
        ras.device_discovered(&remote_device1);
        ras.device_discovered(&remote_device2);

        assert_eq!(listener.remote_ids.len(), 2);
        assert_eq!(listener.remote_ids[0], 42);
        assert_eq!(listener.remote_ids[1], 13);
    }

    #[test]
    fn single_listener_lost_notification() {
        let mut listener = TestListener::new();
        let mut ras = RemoteAuthService::new();
        let remote_device = RemoteDevice { id: 42 };

        ras.add_listener(&mut listener);
        ras.device_discovered(&remote_device);
        ras.device_lost(&remote_device);

        assert_eq!(listener.remote_ids.len(), 0);
    }

    #[test]
    fn all_listeners_lost_notification() {
        let mut listener1 = TestListener::new();
        let mut listener2 = TestListener::new();
        let mut ras = RemoteAuthService::new();
        let remote_device = RemoteDevice { id: 42 };

        ras.add_listener(&mut listener1);
        ras.add_listener(&mut listener2);
        ras.device_discovered(&remote_device);
        ras.device_lost(&remote_device);

        assert_eq!(listener1.remote_ids.len(), 0);
        assert_eq!(listener2.remote_ids.len(), 0);
    }

    #[test]
    fn listeners_lost_notification_for_all_devices() {
        let mut listener = TestListener::new();
        let mut ras = RemoteAuthService::new();
        let remote_device1 = RemoteDevice { id: 42 };
        let remote_device2 = RemoteDevice { id: 13 };
        let remote_device3 = RemoteDevice { id: 99 };

        ras.add_listener(&mut listener);
        ras.device_discovered(&remote_device1);
        ras.device_discovered(&remote_device2);
        ras.device_discovered(&remote_device3);

        ras.device_lost(&remote_device2);
        ras.device_lost(&remote_device3);

        assert_eq!(listener.remote_ids.len(), 1);
        assert_eq!(listener.remote_ids[0], 42);
    }

    #[test]
    fn single_listener_timeout_notification() {
        let mut listener = TestListener::new();
        let mut ras = RemoteAuthService::new();

        ras.add_listener(&mut listener);
        ras.timed_out();

        assert!(listener.got_timeout);
    }

    #[test]
    fn all_listeners_timeout_notification() {
        let mut listener1 = TestListener::new();
        let mut listener2 = TestListener::new();
        let mut ras = RemoteAuthService::new();

        ras.add_listener(&mut listener1);
        ras.add_listener(&mut listener2);
        ras.timed_out();

        assert!(listener1.got_timeout);
        assert!(listener2.got_timeout);
    }

    #[test]
    fn listener_can_be_removed() {
        let mut listener = TestListener::new();
        let mut ras = RemoteAuthService::new();
        let remote_device = RemoteDevice { id: 42 };

        let id = ras.add_listener(&mut listener);
        ras.remove_listener(id);
        ras.device_discovered(&remote_device);

        assert_eq!(listener.remote_ids.len(), 0);
    }

    #[test]
    fn listener_can_be_removed_after_notification() {
        let mut listener = TestListener::new();
        let mut ras = RemoteAuthService::new();
        let remote_device = RemoteDevice { id: 42 };

        let id = ras.add_listener(&mut listener);
        ras.device_discovered(&remote_device);
        ras.remove_listener(id);
        ras.device_discovered(&remote_device);

        assert_eq!(listener.remote_ids.len(), 1);
        assert_eq!(listener.remote_ids[0], 42);
    }
}
