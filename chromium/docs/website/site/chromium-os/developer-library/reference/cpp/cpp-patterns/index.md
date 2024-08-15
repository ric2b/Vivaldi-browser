---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: cpp-patterns
title: C++ Design Patterns
---

[TOC]

Here are some patterns that are especially prevalent in Chromium.

## Abstract base class, Impl, Fake

An extremely common pattern found throughout Chromium is to declare a base
class, e.g. `class Foo`, whose methods are “pure virtual”, i.e. `virtual void
DoTheThing() = 0;`. These abstract classes are usually accompanied by a single
implementation with the suffix “-Impl”, e.g. `class FooImpl : public Foo`.

You might wonder, why bother with an abstract base class if there is only one
implementation? More often than not, the purpose is to facilitate unit testing
by allowing for dummy implementations called fakes, e.g. `class FakeFoo : public
Foo`. The idea is that if you want to test another class that depends on this
class, then you can [inject](https://en.wikipedia.org/wiki/Dependency_injection)
a fake.

```cpp
// foo.h
class Foo {
 public:
  virtual void DoTheThing() = 0;
};

// foo_impl.h
class FooImpl : public Foo {
 public:
  void DoTheThing() override;
};

// fake_foo.h
class FakeFoo : public Foo {
 public:
  void DoTheThing() override { do_the_thing_called_ = true; }

  bool do_the_thing_called_ = false;
};

// bar.h
class Bar {
 public:
  // This constructor injects a Foo instance.
  Bar(std::unique_ptr<Foo> foo);

  void DoSomeThings() {
    foo_->DoTheThing();
    // Do some other things ...
  }

 private:
  std::unique_ptr<Foo> foo_;
};

// bar_test.cc
class BarTest : public testing::Test {
 public:
  void SetUp() override {
    auto fake_foo = std::make_unique<FakeFoo>();
    fake_foo_ = fake_foo.get();
    bar_ = std::make_unique<Bar>(std::move(fake_foo));
  }

  std::unique_ptr<Bar> bar_;
  FakeFoo* fake_foo_;
};

TEST_F(BarTest, DoSomeThings) {
  bar_->DoSomeThings();
  EXPECT_TRUE(foo_->do_the_thing_called_);
}
```

### Why not GMock?

Mocks are an alternative to fakes that rely instead on a mocking framework (in
Chromium, this would be
[GMock](https://g3doc.corp.google.com/third_party/googletest/g3doc/gmock_overview.md)).
Tests written with mocks have a tendency to overspecify the interactions between
objects, which can result in brittle tests that need to be revised with every
small change to the code under test. Oftentimes, introducing an abstract base
class to allow for fakes results in a cleaner design anyway.

GMock also suffers from some technical limitations. It cannot forward move-only
types to mocked methods, so you cannot use it to mock methods that accept a
`std::unique_ptr`, for example.

Mocks are not banned, but our team strongly prefers to use fakes.

## Factories

Oftentimes, we need the ability to exercise more control over the construction
of objects. This is where factories come in.

### Abstract factory class

Suppose one class Foo makes instances of another class Bar. We may want to have
Foo make fake Bars when we're writing unit tests for Foo. One way to achieve
this is to add a factory class for Bar, and then inject it into Foo to control
how Foo makes Bars.

```cpp
// foo.h
class Foo {
 public:
  Foo(std::unique_ptr<Bar::Factory> bar_factory) :
      bar_factory_(std::move(bar_factory)) {
    // Instead of calling Bar's constructor directly, Foo uses the factory
    // whenever it wants to make a Bar.
    bar_ = bar_factory_->Create(42, false);
  }

  void UseTheBar() {
    bar_->DoSomething();
  }

 private:
  std::unique_ptr<Bar::Factory> bar_factory_;
  std::unique_ptr<Bar> bar_;
};

// bar.h
class Bar {
 public:
  class Factory {
   public:
    virtual std::unique_ptr<Bar> Create(int arg1, bool arg2) {
      return base::WrapUnique(new Bar(arg1, arg2));
    }
  };

  virtual void DoSomething();

 private:
  // This constructor can be private to force consumers to use the factory.
  Bar(int arg1, bool arg2);
};

// fake_bar.h
class FakeBar {
 public:
  class Factory : public Bar::Factory {
   public:
    std::unique_ptr<Bar> Create(int arg1, bool arg2) override {
      auto bar = base::WrapUnique(new FakeBar(arg1, arg2));
      last_created_instance_ = bar.get();
      return std::move(bar);
    }

    void DoSomething() override {
      do_something_called_ = true;
    }

    FakeBar* last_created_instance_ = nullptr;
    bool do_something_called_ = false;
  };
};

// foo_test.cc
class FooTest : public testing::Test {
 public:
  void SetUp() override {
    auto bar_factory = std::make_unique<FakeBar::Factory>();
    bar_factory_ = bar_factory.get();
    foo_ = std::make_unique<Foo>(std::move(bar_factory));
  }

  std::unique_ptr<Foo> foo_;
  FakeBar::Factory* bar_factory_ = nullptr;
};

TEST_F(FooTest, UseTheBar) {
  foo_.UseTheBar();
  EXPECT_TRUE(bar_factory_->last_created_instance_);
  EXPECT_TRUE(bar_factory_->last_created_instance_->do_something_called_);
}
```

#### Alternative: static factory instance

A variant of this pattern is to add a static factory method and only override
the factory for testing. This has the advantage of eliminating the need to
inject the factory into Foo, but has the large drawback of introducing a global
variable, which could lead to test failures if tests don't clean up after
themselves.

This is commonly used in our team's code, but do take care to clean up your
static variables when tearing down tests.

```cpp
class Bar {
 public:
  class Factory {
   public:
    static std::unique_ptr<Bar> Create(int arg1, bool arg2) {
      if (bar_factory_for_testing_) {
        return bar_factory_for_testing_->CreateInstance(arg1, arg2);
      }
      return base::WrapUnique(new Bar(arg1, arg2));
    }

    static void SetFactoryForTesting(Factory* factory);

   private:
    virtual std::unique_ptr<Bar> CreateInstance(int arg1, bool arg2) = 0;

    static Factory* bar_factory_for_testing_ = nullptr;
  };
};
```

### Factory method

If the construction of a class can fail or take a long time, then the
constructor should not be exposed directly. Constructors have no way of
signaling failure, and no way to delay returning the object without blocking the
thread. Instead, we can use factory methods to expose an api that accounts for
these nuances.

Note: This situation comes up pretty frequently. It is
[common practice](https://en.cppreference.com/w/cpp/language/raii) to use
objects in C++ to represent acquired resources (file handles, pipes,
connections, hardware interfaces, etc.), and it is convenient to not have
"partially constructed" objects for resources that take time to acquire.

As an example, suppose we have a Connection class that represents an active
connection to a remote device. Establishing this connection could fail for
numerous reasons, and will introduce a fair amount of latency. We can use a
factory method to present a friendly interface for building these connections:

```cpp
class Connection {
 public:
  enum class Result {
    kSuccess,
    kTimeout,
    kConnectionRejected,
  };
  using ConnectCallback = base::OnceCallback<void(Result, std::unique_ptr<Connection>)>;

  // This factory method creates Connections asynchronously. May return nullptr if
  // the result is not kSuccess.
  static void Connect(ConnectCallback callback);

 private:
  // This constructor can be private since we want consumers of this api to use the
  // |Connect()| method.
  Connection();
};
```

## Observers

Ordinarily, if object A wants to communicate with object B, it simply calls
methods on object B. There are lots of situations where this isn't convenient:

-   A doesn't know about B, e.g. if A is part of a library that B is using.
-   A needs to send messages to lots of Bs.
-   A doesn't know the lifetime of B.
-   A needs to send the same message to lots of different kinds of objects, not
    just B.

In this case, it's better to have B subscribe to receive messages from A, and
unsubscribe when it no longer needs to receive the messages. The Observer
pattern is used to achieve this in Chromium.

```cpp
class Foo {
 public:
  // This interface defines what kinds of events the observer can receive. This
  // inherits from CheckedObserver in order to be compatible with ObserverList.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnFooEvent() = 0;
  };

  // Each observer uses these methods to subscribe/unsubscribe to events.
  AddObserver(Observer* observer) { observers_.AddObserver(observer); }
  RemoveObserver(Observer* observer) { observers_.RemoveObserver(observer); }

 private:
   // Foo calls this method whenever the event occurs in order to notify observers.
   void NotifyFooEvent() {
     for (Observer& obs : observers_) {
       obs.OnFooEvent();
     }
   }

   // This is the collection of observers. ObserverList does some extra magic to
   // ensure that if the observer is destroyed without being removed first, then
   // use-after-free does not occur.
   base::ObserverList<Observer> observers_;
};

// Example observer
class Bar : public Foo::Observer {
 public:
  Bar(Foo* foo) : foo_(foo) {
    foo_->AddObserver(this);
  }

  ~Bar() {
    foo_->RemoveObserver(this);
  }

 private:
  // Foo::Observer:
  void OnFooEvent() override {
    LOG(INFO) << "Received FooEvent";
  }

  Foo* foo_ = nullptr;
};
```

## Delegates

Delegates are used to inject dependencies in situations where a direct
dependency would be disallowed:

-   If module A already depends on module B, then module B cannot depend on
    module A, since this would introduce a dependency cycle.
-   Certain directories in Chromium are forbidden from depending on certain
    other directories (These relationships are defined in the DEPS files). For
    example, code in the `//ash` and `//chromeos` directories may not depend on
    code in `//chrome`.

Read more about the Delegate Pattern
[here](/chromium-os/developer-library/guides/cpp/resolving-dependency-issues).
