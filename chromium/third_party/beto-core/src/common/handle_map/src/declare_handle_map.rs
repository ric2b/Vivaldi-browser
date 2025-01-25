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

//! Implementation of the `declare_handle_map!` macro

#[macro_export]
/// ```ignore
/// declare_handle_map! (
///     $handle_module_name,
///     $map_dimension_provider,
///     $handle_type_name,
///     $wrapped_type,
/// )
/// ```
///
/// Declares a new public module with name `handle_module_name` which implements handle functionality
/// for the given struct `handle_type_name` which is `#[repr(C)]` and represents FFI-accessible
/// handles to values of type `wrapped_type`. `handle_type_name` expects a struct with a single u64
/// field `handle_id`.
///
/// Internal to the generated module, a new static `SingletonHandleMap` is created, where the
/// maximum number of active handles and the number of shards are given by
/// the dimensions returned by evaluation of the `map_dimension_provider` expression.
///
/// Note: `map_dimension_provider` will be evaluated within the defined module's scope,
/// so you will likely need to use `super` to refer to definitions in the enclosing scope.
///
/// # Example
/// The following code defines an FFI-safe type `StringHandle` which references
/// the `String` data-type, and uses it to define a (contrived)
/// function `sample` which will print "Hello World".
///
/// ```
/// use core::ops::Deref;
/// use handle_map::{declare_handle_map, HandleMapDimensions, HandleLike};
///
/// fn get_string_handle_map_dimensions() -> HandleMapDimensions {
///     HandleMapDimensions {
///         num_shards: 8,
///         max_active_handles: 100,
///     }
///  }
///
///  struct StringHandle {
///     handle_id: u64
///  }
///
///  declare_handle_map!(
///     string_handle,
///     super::get_string_handle_map_dimensions(),
///     super::StringHandle,
///     String
///  );
///
///  fn main() {
///         // Note: this method could panic if there are
///         // more than 99 outstanding handles.
///
///         // Allocate a new string-handle pointing to the string "Hello"
///         let handle = StringHandle::allocate(|| { "Hello".to_string() }).unwrap();
///         {
///             // Obtain a write-guard on the contents of our handle
///             let mut handle_write_guard = handle.get_mut().unwrap();
///             handle_write_guard.push_str(" World");
///             // Write guard is auto-dropped at the end of this block.
///         }
///         {
///             // Obtain a read-guard on the contents of our handle.
///             // Note that we had to ensure that the write-guard was
///             // dropped prior to doing this, or else execution
///             // could potentially hang.
///             let handle_read_guard = handle.get().unwrap();
///             println!("{}", handle_read_guard.deref());
///         }
///         // Clean up the data behind the created handle
///         handle.deallocate().unwrap();
///  }
///
/// ```
macro_rules! declare_handle_map {
    (
        $handle_module_name:ident,
        $map_dimension_provider:expr,
        $handle_type_name:ty,
        $wrapped_type:ty
     ) => {
        #[doc = ::core::concat!(
                    "Macro-generated (via `handle_map::declare_handle_map!`) module which",
                    " defines the `", ::core::stringify!($handle_module_name), "::",
                    ::core::stringify!($handle_type_name), "` FFI-transmissible handle type ",
                    " which references values of type `", ::core::stringify!($wrapped_type), "`."
                )]
        pub mod $handle_module_name {
            $crate::reexport::lazy_static::lazy_static! {
                static ref GLOBAL_HANDLE_MAP: $crate::HandleMap<$wrapped_type> =
                $crate::HandleMap::with_dimensions($map_dimension_provider);
            }

            pub (crate) fn get_current_allocation_count() -> u32 {
                GLOBAL_HANDLE_MAP.get_current_allocation_count()
            }

            #[doc = ::core::concat!(
                        "A `#[repr(C)]` handle to a value of type `",
                        ::core::stringify!($wrapped_type), "`."
                    )]

            impl $handle_type_name {
                /// Cast the given raw Handle to this HandleLike
                pub fn from_handle(handle: $crate::Handle) -> Self {
                    Self { handle_id: handle.get_id() }
                }

                /// Get this HandleLike as a raw Handle.
                pub fn get_as_handle(&self) -> $crate::Handle {
                    $crate::Handle::from_id(self.handle_id)
                }
            }
            impl $crate::HandleLike for $handle_type_name {
                type Object = $wrapped_type;
                fn try_allocate<E: core::fmt::Debug>(
                    initial_value_provider: impl FnOnce() -> Result<$wrapped_type, E>,
                ) -> Result<Self, $crate::HandleMapTryAllocateError<E>> {
                    GLOBAL_HANDLE_MAP
                        .try_allocate(initial_value_provider)
                        .map(|derived_handle| Self { handle_id: derived_handle.get_id() })
                }
                fn allocate(
                    initial_value_provider: impl FnOnce() -> $wrapped_type,
                ) -> Result<Self, $crate::HandleMapFullError> {
                    GLOBAL_HANDLE_MAP
                        .allocate(initial_value_provider)
                        .map(|derived_handle| Self { handle_id: derived_handle.get_id() })
                }
                fn get(
                    &self,
                ) -> Result<$crate::ObjectReadGuardImpl<$wrapped_type>, $crate::HandleNotPresentError>
                {
                    GLOBAL_HANDLE_MAP.get(self.get_as_handle())
                }
                fn get_mut(
                    &self,
                ) -> Result<
                    $crate::ObjectReadWriteGuardImpl<$wrapped_type>,
                    $crate::HandleNotPresentError,
                > {
                    GLOBAL_HANDLE_MAP.get_mut(self.get_as_handle())
                }
                fn deallocate(self) -> Result<$wrapped_type, $crate::HandleNotPresentError> {
                    GLOBAL_HANDLE_MAP.deallocate(self.get_as_handle())
                }
            }
        }
    };
}
