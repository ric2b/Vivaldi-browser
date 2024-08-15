---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: cpp-memory-management
title: C++ Memory Management
---

[TOC]

## Common Bugs

C++ is not memory-safe. The language provides a number of primitives that, when
used incorrectly or with insufficient consideration for object lifetimes, can
result in crashes or undefined behavior. These are often very serious bugs,
since they can readily be used in security exploits. Let’s look at the most
common bug types:

### Use After Free (UAF)

When a reference (`Foo&`) or pointer (`Foo*`) to an object is used after the
object has already been destroyed, the resulting bug is called “use after free”.
In the best case, the result will be a segfault, and the program will simply
crash. If the freed memory has already been reallocated, though, then the
program may continue running with undefined behavior.

Example:

```cpp
Foo* GetFoo() {
  Foo foo; // |foo| is allocated on the stack
           // and will be destroyed when the function returns.
  return &foo;
}

void main() {
  Foo* foo_ptr = GetFoo();
  foo_ptr->DoSomething(); // The Foo has already been destroyed.
}
```

To avoid this bug, [smart pointers](#smart-pointers) (e.g. `std::unique_ptr`,
`base::WeakPtr`) should be preferred to raw pointers and references whenever
memory is heap allocated. Never pass around raw pointers unless you can
guarantee that the object will outlive any calls against it.

Passing raw pointers/references can be OK, for example, when object A owns
object B, and A passes a A* to B.

```cpp
// Good:
class Foo {
 public:
  void CreateBar() {
    // It's OK to pass a raw pointer here, since Foo owns the Bar, and Foo is
    // guaranteed to live longer than Bar.
    bar_ = std::make_unique<Bar>(/*foo_ptr=*/this);
  }

 private:
  std::unique_ptr<Bar> bar_;
};
```

But if Foo does not keep ownership of Bar, that same example is much more
problematic:

```cpp
// Bad:
class Foo {
 public:
  std::unique_ptr<Bar> CreateBar() {
    // This is dangerous! Foo gives ownership of the Bar to the caller of this
    // function, so there is no guarantee that the Foo won't be destroyed
    // before Bar.
    return std::make_unique<Bar>(/*foo_ptr=*/this);
  }
};
```

### Buffer Overflow

C and C++ allow the programmer to perform arithmetic operations on pointers as a
means to traverse data structures like arrays. These operations are not
guaranteed to be safe, so arithmetic errors when writing to an array with a raw
pointer can result in writing to neighboring regions of memory. This can lead to
exploitable bugs.

Consider, what’s the worst-case scenario for code like the following?

```cpp
class Bar {
 public:
  void GetNameFromQuestionableSource() {
    char* name = HttpGet("http://evil.com/name");
    char* ptr = name_;
    while (name) {
      (*ptr) = *name;
      ptr++;
      name++;
    }
    std::move(callback_).Run();
  }

 private:
  void FireTheMissiles();

  char[10] name_;
  base::OnceClosure callback_;
};
```

Since the bounds of the char array are not checked, an attacker could write past
the end of the array and put whatever memory address they like into the
callback. `&Bar::FireTheMissiles` might be an interesting choice.

So, how can we avoid this kind of bug?

-   Always prefer using safer data structures like `std::vector`, `std::array`,
    or `std::string` over raw arrays.
-   Avoid performing arithmetic on pointers. Use iterators or indices instead.
-   It’s never a bad idea to check the size of the container you’re writing
    into.

### Invalid Iterator

Another related problem to be wary of is that modifying a container usually
invalidates any iterators on that container. E.g.

```cpp
void main() {
  std::vector<int> nums = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  for (auto it = nums.begin(); it != nums.end(); it++) {
    if ((*it) % 2 == 0) {
      nums.erase(it); // Erasing from the vector invalidates the iterator.
    }
  }
}
```

There are a number of ways to avoid this issue:

-   Keep a separate list of the modifications you want to make, and perform them
    afterwards.

    ```cpp
    void main() {
      std::vector<int> nums = {1, 2, 3, 4, 5, 6, 7, 8, 9};
      std::vector<int> new_nums;
      for (auto it = nums.begin(); it != nums.end(); it++) {
        if ((*it) % 2 == 0) {
          continue;
        }
        new_nums.push_back(*it);
      }
      nums.swap(new_nums);
    }
    ```

-   Make use of [algorithms](https://en.cppreference.com/w/cpp/algorithm/remove)
    from the standard library.

    ```cpp
    void main() {
      std::vector<int> nums = {1, 2, 3, 4, 5, 6, 7, 8, 9};
      nums.erase(std::remove_if(nums.begin(), nums.end(),
                                [](int n) { return n % 2 == 0; }),
                 nums.end())
    }
    ```

-   Some methods, e.g. `std::vector::erase`, will return a new iterator pointing
    to the next element after the erased element.

    ```cpp
    void main() {
      std::vector<int> nums = {1, 2, 3, 4, 5, 6, 7, 8, 9};
      auto it = nums.begin();
      while (it != nums.end()) {
        if ((*it) % 2 == 0) {
          // Use the return value of |erase| to update |it|.
          it = nums.erase(it);
        } else {
          it++;
        }
      }
    }
    ```

## Smart Pointers

Traditionally, when you
[heap-allocate](https://courses.engr.illinois.edu/cs225/fa2022/resources/stack-heap/)
memory in a C-like language, you are responsible for releasing that memory when
you're finished with it.

```cpp
// DO NOT DO THIS
// The "new" keyword allocates memory for a Foo on the heap and returns a
// pointer to the newly initialized Foo.
Foo* foo = new Foo();

foo->DoSomething();

// Here we're giving |bar| a reference to Foo, but it's not clear who owns
// the Foo.
bar.SetFoo(foo);

// The "delete" keyword calls the Foo destructor and releases the memory.
delete foo;
```

This is extremely error-prone:

-   If you forget to delete the Foo, that could cause a memory leak, or even
    leak other resources that Foo might have acquired.
-   If you delete the Foo too soon, or fail to notify every class that still has
    a pointer to the Foo, then you could cause a use-after-free bug.
-   If you fail to initialize the Foo, making calls against the uninitialized
    pointer could result in undefined behavior or a segfault.

Contrast this with how easy it is to use stack memory correctly:

```cpp
void main() {
  Foo foo;
  Bar bar(&foo);

  foo.DoSomething();

  // The Foo and the Bar are automatically destroyed when they fall out of
  // scope at the end of this function. They are destroyed in the reverse order
  // of declaration, so we don't need to worry about the Bar living longer than
  // its Foo.
}
```

Smart pointers help to solve all of these issues for heap memory, and get us
much closer to the ease-of-use of stack memory.

### `std::unique_ptr<T>`

Unique pointers represent sole ownership of the object they point to. When the
unique pointer is destroyed, the object it references is automatically
destroyed.

```cpp
void main() {
  // The Foo is heap-allocated here. |make_unique| allows us to create the Foo
  // without ever having an unprotected raw pointer.
  std::unique_ptr<Foo> foo = std::make_unique<Foo>();

  // The |get()| method can be used to access a raw pointer to the Foo. Since
  // we're using smart pointers, notice that there's no longer any ambiguity
  // about whether we're transferring ownership to Bar. This is also guaranteed
  // to be safe, since the Foo will outlive the Bar.
  Bar bar;
  bar.UseTheFoo(foo.get());

  // The Foo is automatically destroyed here when the unique pointer falls out
  // of scope.
}
```

Function signatures containing unique pointers make the transfer of ownership of
an object explicit.

```cpp
class Bar {
 public:
  // Since this takes a unique pointer, it's obvious that this takes ownership
  // of the Foo away from whatever calls it.
  void SetFoo(std::unique_ptr<Foo> foo);
};

// Since this returns a unique pointer, it's obvious that this is giving
// ownership over to whatever calls it.
std::unique_ptr<Foo> CreateFoo();
```

### `base::WeakPtr<T>`

Like raw pointers, weak pointers represent a non-owner reference to an object.
The difference is that weak pointers track whether the object they reference has
been destroyed or not, allowing the caller to check whether or not it is safe to
make calls against the pointer.

```cpp
class Foo {
 public:
  void DoSomething();

  base::WeakPtr<Foo> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

 private:
  // Weak pointers are always created by a factory object bound to the lifetime
  // of the object they reference. When the factory is destroyed, it
  // automatically invalidates any existing weak pointers.
  base::WeakPtrFactory<Foo> weak_ptr_factory_{this};
};

void main() {
  ...
  // Regardless of where we managed to get a weak pointer to this Foo, we can
  // check whether the Foo is still alive by seeing whether the weak pointer is
  // falsey.
  if (foo_weak_ptr) {
    // This call is now safe, since we've checked that the Foo exists.
    foo_weak_ptr->DoSomething();
  }
  ...
}
```

Function signatures containing weak pointers make it explicit that (1) we are
NOT transferring any ownership and (2) we make no guarantees about the lifetime
of the object.

```cpp
class Bar {
 public:
  // It's obvious that this Bar does not own the Foo, even though it stores a
  // reference to the Foo. If the Foo is invalid by the time the Bar needs it,
  // then the Bar has to get a new Foo, or handle the error condition.
  void Bar(base::WeakPtr<Foo> foo) : foo_(std::move(foo)) {}

 private:
  base::WeakPtr<Foo> foo_;
}
```

### `base::ScopedRefPtr<T>`

This is roughly equivalent to `std::shared_ptr<T>`, which is
[banned in Chromium](https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++-features.md#std_shared_ptr-banned).
Shared pointers represent shared ownership an object. Internally, they're
implemented using
[reference counting](https://en.wikipedia.org/wiki/Reference_counting). The
object is kept alive as long as any shared pointer to it is still alive, and
once the last shared pointer is destroyed, the object is destroyed.

Shared pointers are most frequently used to allow several objects to share usage
of an expensive resource. Once all references are destroyed, the resource is
automatically released.

Shared pointers should be avoided as much as reasonably possible. They make it
easy to make objects that live much longer than necessary.

### `T*` and `base::raw_ptr<T>`

Even with smart pointers, raw pointers still see widespread use in Chromium.
When you see a raw pointer in a function signature or a class member, that's
a strong indication that (1) ownership is not being transferred, and (2) the
author of the code wanted to convey that they did the homework to ensure that
the lifetime of this object is being managed correctly.

When passing a raw pointer to a function, the default assumption is that the
referenced object will exist for the entire duration of the function call (i.e.
the object is owned by something higher in the call stack).

When storing a raw pointer in a member variable, it is assumed that the
referenced object lives longer than the class that stores the pointer. This is
typically guaranteed by ownership (if A owns B, then B can store a pointer to
A), but could also be guaranteed if the referenced object is, say, a global
variable valid throughout the entire life of the program.

`base::raw_ptr<T>` is a wrapper around a raw pointer designed to mitigate UAF
bugs. It should be used in place of a C-style pointer `T*` whenever raw pointers
are stored as member variables. See
[the docs](https://chromium.googlesource.com/chromium/src/+/HEAD/base/memory/raw_ptr.md)
for more info.

