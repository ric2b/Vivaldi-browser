// Copyright 2024 Google LLC
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

//! Utilities for handling errors and propagating them into exceptions in Java.

use jni::{
    descriptors::Desc,
    objects::{JClass, JThrowable},
    JNIEnv,
};
use std::fmt::Debug;

/// An error that can be thrown as a Java exception, or an error in the JNI layer.
///
/// This allows using Rust's error propagation mechanism (the `?` operator) to surface both JNI
/// errors and Java throwables, and convert them to Java exceptions at the top level. At the top
/// level of a JNI function implementation, it can use [`ThrowableJniResultExt::unwrap_or_throw`] to
/// convert the error to Java exception, or terminate the JVM with a fatal error in the case of JNI
/// errors.
pub enum ThrowableJniError<'local> {
    /// A java throwable object instance. The throwable can be received from the Java side, or
    /// created using [`call_constructor`][crate::call_constructor].
    JavaThrowable(JThrowable<'local>),
    /// A [`jni::errors::Exception`], which contains the class and the message to be able to create
    /// the exception to be thrown.
    JavaException(jni::errors::Exception),
    /// An error from the [`jni`] crate.
    JniError(jni::errors::Error),
}

impl<'local> ThrowableJniError<'local> {
    /// Throws the error as a Java exception.
    ///
    /// If the error is a `JavaThrowable` or a `JavaException`, it will be thrown on the given
    /// `env`. If the error is a `JniError`, this will turn it into a [`JNIEnv::fatal_error`],
    /// unless the error type is [`jni::errors::Error::JavaException`], in which case the error will
    /// be ignored, allowing the already-thrown exception to remain in JVM.
    ///
    /// In typical usages, callers should return some default value immediately after calling this.
    /// See the example in [`try_throwable`].
    pub fn throw_on_jvm(self, env: &mut JNIEnv<'_>) {
        match self {
            ThrowableJniError::JavaThrowable(throwable) => match env.throw(throwable) {
                Ok(()) => {}
                Err(jni::errors::Error::JavaException) => {
                    let _ = env.exception_describe();
                    env.fatal_error("Throwing exception failed");
                }
                Err(_) => env.fatal_error("Throwing exception failed"),
            },
            ThrowableJniError::JavaException(exception) => match env.throw(exception) {
                Ok(()) => {}
                Err(jni::errors::Error::JavaException) => {
                    let _ = env.exception_describe();
                    env.fatal_error("Throwing exception failed");
                }
                Err(_) => env.fatal_error("Throwing exception failed"),
            },
            ThrowableJniError::JniError(err) => {
                match err {
                    jni::errors::Error::JavaException => {
                        // Ignore the exception, it's already pending in the JVM
                    }
                    _ => env.fatal_error(format!("{err}")),
                }
            }
        }
    }
}

impl Debug for ThrowableJniError<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::JavaThrowable(_) => f.debug_tuple("JavaThrowable").finish(),
            Self::JavaException(arg0) => f
                .debug_struct("JavaException")
                .field("class", &arg0.class)
                .field("msg", &arg0.msg)
                .finish(),
            Self::JniError(arg0) => f.debug_tuple("JniError").field(arg0).finish(),
        }
    }
}

impl<'local> From<JThrowable<'local>> for ThrowableJniError<'local> {
    fn from(value: JThrowable<'local>) -> Self {
        Self::JavaThrowable(value)
    }
}

impl From<jni::errors::Error> for ThrowableJniError<'_> {
    fn from(value: jni::errors::Error) -> Self {
        Self::JniError(value)
    }
}

impl From<jni::errors::Exception> for ThrowableJniError<'_> {
    fn from(value: jni::errors::Exception) -> Self {
        Self::JavaException(value)
    }
}

/// Extension trait for `Result<T, ThrowableJniError>`.
pub trait ThrowableJniResultExt<T> {
    /// Returns the contained `Ok` value, or throw an exception on the JVM and return the default.
    ///
    /// Consumes the `self` argument then, if `Ok`, returns the contained value. Otherwise, if
    /// `Err`, the error will be thrown on the JVM using [`ThrowableJniError::throw_on_jvm`] and a
    /// default value will be returned. The value returned by `default` typically does not matter,
    /// because while there is an exception pending on the JVM, the return value from your JNI
    /// method is ignored.
    fn unwrap_or_throw(self, env: &mut JNIEnv<'_>) -> T
    where
        T: Default,
        Self: Sized,
    {
        self.unwrap_or_throw_with_default(env, Default::default)
    }

    /// Returns the contained `Ok` value, or throw an exception on the JVM and return some default.
    ///
    /// Consumes the `self` argument then, if `Ok`, returns the contained value. Otherwise, if
    /// `Err`, the error will be thrown on the JVM using [`ThrowableJniError::throw_on_jvm`] and the
    /// value from `default` will be returned. The value returned by `default` typically does not
    /// matter, because while there is an exception pending on the JVM, the return value from your
    /// JNI method is ignored.
    fn unwrap_or_throw_with_default(self, env: &mut JNIEnv<'_>, default: impl FnOnce() -> T) -> T;
}

impl<'local, T> ThrowableJniResultExt<T> for Result<T, ThrowableJniError<'local>> {
    fn unwrap_or_throw_with_default(self, env: &mut JNIEnv<'_>, default: impl FnOnce() -> T) -> T {
        match self {
            Ok(v) => v,
            Err(e) => {
                e.throw_on_jvm(env);
                default()
            }
        }
    }
}

/// Creates a `NullPointerException` with a default error message.
pub fn null_pointer_exception() -> jni::errors::Exception {
    jni::errors::Exception {
        class: "java/lang/NullPointerException".into(),
        msg: "Null pointer".into(),
    }
}

/// Creates a runtime exception with the given message.
pub fn runtime_exception(msg: impl ToString) -> jni::errors::Exception {
    jni::errors::Exception {
        class: "java/lang/RuntimeException".into(),
        msg: msg.to_string(),
    }
}

/// Runs the given `func` and turns any [`ThrowableJniError`] into Java exceptions.
///
/// A convenience function that takes a closure `func`, runs it immediately, and calls
/// [`ThrowableJniResultExt::unwrap_or_throw`] on the result. This allows the code inside the
/// closure to propagate [`ThrowableJniErrors`][ThrowableJniError] using the `?` operator.
///
/// # Example
///
/// ```
/// use pourover::{jni_method, exception::{runtime_exception, try_throwable}};
/// use jni::{JNIEnv, objects::{JByteArray, JClass}, sys::{jboolean, JNI_TRUE}};
///
/// #[jni_method(package = "com.example", class = "Foo")]
/// extern "system" fn nativeMaybeThrowException<'local>(
///     mut env: JNIEnv<'local>,
///     _cls: JClass<'local>,
///     value: JByteArray<'local>,
/// ) -> jboolean {
///     try_throwable(&mut env, |env| {
///         let value = env.convert_byte_array(value)?;
///         if value == b"Throw exception" {
///             Err(runtime_exception("Argument was \"throw exception\""))?
///         }
///         Ok(JNI_TRUE)
///     })
/// }
/// ```
pub fn try_throwable<'local, R: Default>(
    env: &mut JNIEnv<'local>,
    func: impl FnOnce(&mut JNIEnv<'local>) -> Result<R, ThrowableJniError<'local>>,
) -> R {
    func(env).unwrap_or_throw(env)
}

/// Extension trait on `jni::errors::Result<T>`.
pub trait JniResultExt<T> {
    /// Extracts an exception of the given class from a [`jni::errors::Result`].
    ///
    /// If `self` is `Err(JavaException)` and the pending exception on the JVM matches the given
    /// `desc`, the exception will be cleared on the JVM and the associated throwable will be
    /// returned.
    ///
    /// # Returns
    /// A nested result object. If `self` contains an exception matching the given `desc`, this will
    /// return `Ok(Err(throwable))`. If `self` is `Ok`, this will return `Ok(Ok(value))`. Otherwise
    /// if there are other errors or non-matching exceptions, the error will be propagated as
    /// `Err(jni_error)`.
    fn extract_exception<'local>(
        self,
        env: &mut JNIEnv<'local>,
        desc: impl Desc<'local, JClass<'local>>,
    ) -> jni::errors::Result<Result<T, JThrowable<'local>>>;
}

impl<T> JniResultExt<T> for jni::errors::Result<T> {
    fn extract_exception<'local>(
        self,
        env: &mut JNIEnv<'local>,
        desc: impl Desc<'local, JClass<'local>>,
    ) -> jni::errors::Result<Result<T, JThrowable<'local>>> {
        match self {
            Ok(v) => Ok(Ok(v)),
            Err(jni::errors::Error::JavaException) => {
                let throwable = env.exception_occurred()?;
                // Need to clear the exception ahead of time, otherwise `is_instance_of` will fail
                env.exception_clear()?;
                if env.is_instance_of(&throwable, desc)? {
                    Ok(Err(throwable))
                } else {
                    env.throw(throwable)?;
                    Err(jni::errors::Error::JavaException)
                }
            }
            Err(e) => Err(e),
        }
    }
}
