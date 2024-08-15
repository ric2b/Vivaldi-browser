---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: cpp-mojo
title: Using Mojo in C++
---

[TOC]

## Intro

Mojo is Chromium's IPC (inter-process communication) framework. Mojo provides a
language-agnostic way of defining interfaces and data structures that can be
[marshalled](https://en.wikipedia.org/wiki/Marshalling_\(computer_science\))
across process boundaries.

Mojo is frequently used to

-   Perform procedure calls between processes (e.g. Browser process and utility
    process)
-   Communicate between C++ and JavaScript (WebUI)
-   Create sandboxes (unprivileged processes, see
    [rule of two](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/rule-of-2.md))

## Resources

-   [Mojo documentation](https://chromium.googlesource.com/chromium/src/+/master/mojo/README.md)
-   [Getting Started with Mojo](https://chromium.googlesource.com/chromium/src/+/HEAD/mojo/README.md#Getting-Started-With-Mojo)

## Stubbing Mojo Pipes

To write tests for Mojo code, you can use Fakes to create test Mojo objects to
stub the message pipe. You can also use `mojo::MakeSelfOwnedReceiver()` to bind
the lifetimes of the Fake and the Mojo endpoint for the test, which will
simplify the code needed to mock. Once the mock endpoint is setup, it can be
injected into the test to be used as the other endpoint. During the test, the
stubbed endpoint will be used to verify that it has received the expected
messages. Using
[bluetooth_classic_medium_unittest.cc](https://source.chromium.org/chromium/chromium/src/+/main:chrome/services/sharing/nearby/platform/bluetooth_classic_medium_unittest.cc;l=1?q=bluetooth_classic_medium_unittest.cc&sq=)
for an example:

-   Setting up the mock Mojo endpoint to be use in the unit tests:

```cpp
void SetUp() override {
  // Build a FakeAdapter instance -- this is a test stub of
  // the adapter.mojom interface.
  auto fake_adapter = std::make_unique<bluetooth::FakeAdapter>();
  fake_adapter_ = fake_adapter.get();

  mojo::PendingRemote<bluetooth::mojom::Adapter> pending_adapter;

  // Build the Mojo pipe to be used for this test. Inject the
  // FakeAdapter instance that the object-under-test will be
  // calling via a Remote<bluetooth::mojom::Adapter>.
  mojo::MakeSelfOwnedReceiver(
        std::move(fake_adapter),
        pending_adapter.InitWithNewPipeAndPassReceiver());

  // Actually Bind the Remote to the Mojo pipe. The
  // object-under-test can now directly consume this
  // Remote<bluetooth::mojom::Adapter>.
  remote_adapter_.Bind(std::move(pending_adapter),
                       /*bind_task_runner=*/nullptr);

  // Inject the Remote<bluetooth::mojom::Adapter> into the object-under-test
  // (BluetoothClassicMedium).
  bluetooth_classic_medium_ =
     std::make_unique<BluetoothClassicMedium>(remote_adapter_);
```

-   Using the mocked objects to verify expectations in the unit tests:

```cpp
  // Make a method call on the object-under-test that will
  // trigger it to call its  Remote<bluetooth::mojom::Adapter>:
  EXPECT_TRUE(bluetooth_classic_medium_->StartDiscovery(
             std::move(discovery_callback_)));

  // Confirm that the Adapter stub instance on the Receiver side
  // of the Mojo pipe was called:
  EXPECT_TRUE(fake_adapter_->IsDiscoverySessionActive());
```

## Mojo Traits

Mojo Traits allow a C++ type to be mapped to a mojo type and vice versa. Using a
Mojo trait guarantees type safety in the conversion, and reduces the overhead in
manually converting between two types.

Mojo provides a range of type mappings, including `StructTraits<>`,
`ArrayTraits<>`, `EnumTraits<>`, and
[more](https://chromium.googlesource.com/chromium/src/+/master/mojo/public/cpp/bindings/README.md#type-mapping).
For example, a C++ enum:

```cpp
enum CakeFlavors {
  kVanilla,
  kChocolate,
  kRedVelvet,
}
```

can be type-mapped to a `.mojom` enum using an `EnumTrait<>`.

1.  Create a `.mojom` enum based on the C++ enum.

    ```
    enum CakeFlavors {
      kVanilla,
      kChocolate,
      kRedVelvet,
    }
    ```

2.  Add `*_mojom_traits.h`:

    Two function signatures need to be added:

    ```cpp
    #include "mojo/public/cpp/bindings/enum_traits.h"

    template <>
    class EnumTraits<CakeFlavors, mojom::CakeFlavors> {
      public:
        static mojom::CakeFlavors ToMojom(CakeFlavors);
        static bool FromMojom(mojom::CakeFlavors input, CakeFlavors output);
    };
    ```

3.  Add `*_mojom_traits.cc`:

    Implement these methods using a `switch` statement. For example:

    ```cpp
    //static
    EnumTraits<mojom::CakeFlavors, CakeFlavors>::
        ToMojom(CakeFlavors input) {
          switch (input) {
            case CakeFlavors::kVanilla:
              return mojom::CakeFlavors::kVanilla;
            case CakeFlavors::kChocolate:
              return mojom::CakeFlavors::kChocolate;
            case CakeFlavors::kRedVelvet:
              return mojom::CakeFlavors::kRedVelvet;
          }

          // Failure to convert should never occur.
          NOTREACHED_NORETURN();
        }

    // static
    EnumTraits<mojom::CakeFlavors, CakeFlavors>::
      FromMojom(mojo::CakeFlavors input, CakeFlavors output) {
          switch (input) {
            case mojom::CakeFlavors::kVanilla:
              *output = CakeFlavors::kVanilla;
              return true;
            case mojom::CakeFlavors::kChocolate:
              *output = CakeFlavors::kChocolate;
              return true;
            case mojom::CakeFlavors::kRedVelvet:
              *output = CakeFlavors::kRedVelvet;
              return true;
          }

          // Return `false` to indicate the conversion was not successful.
          return false;
      }

    ```

4.  Add the Mojom traits to the `BUILD.gn` file:

    ```
    mojom("mojom") {
      sources = [...]

      ...

      cpp_typemaps = [
        {
          types = [
            {
              mojom = "mojom.CakeFlavors"
              cpp = "CakeFlavors"
            },
          ]
          trait_headers = [ "cake_flavors_mojom_traits.h" ]
          trait_sources = [ "cake_flavors_mojom_traits.cc" ]
          traits_deps = [
            ...
          ]
        },
      ]
    }
    ```

5.  Unit testing:

    It is recommended to implicitly test (with unit tests that consume the
    conversion) and manually verify that the Mojo trait mapping works as
    expected. Example: crrev.com/c/2809147.

TIP: Read more about Mojo type mapping in the
[Mojo documentation](https://chromium.googlesource.com/chromium/src/+/master/mojo/public/cpp/bindings/README.md#type-mapping).
