# What is this?

A crypto provider that abstracts over different crypto implementations, mainly the Rust
implementations by [RustCrypto](https://github.com/RustCrypto) and BoringSSL.

## Project structure

### `crypto_provider`

Our own abstraction on top of crypto implementations, including functionalities
like AES, SHA2, X25519 and P256 ECDH, HKDF, HMAC, etc.

Two implementations are currently provided, `crypto_provider_rustcrypto` and
`crypto_provider_boringssl`.

#### `crypto_provider::aes`
Abstraction on top plain AES, including AES-CTR and AES-CBC.

Since we know we'll have multiple AES implementations in practice (an embedded
device might want to use mbed, but a phone or server might use BoringSSL, etc),
it's nice to define our own minimal AES interface that exposes only what we need
and is easy to use from FFI (when we get to that point).

### `crypto_provider_rustcrypto`

Implementations of `crypto_provider` types using the convenient pure-Rust primitives
from [Rust Crypto](https://github.com/RustCrypto).

### `crypto_provider_boringssl`

Implementations of `crypto_provider` types using the
[bssl-crypto](https://boringssl.googlesource.com/boringssl/+/master/rust/bssl-crypto), which is a
Rust binding layer for BoringSSL.

## Setup

See `nearby/presence/README.md` for machine setup instructions.
