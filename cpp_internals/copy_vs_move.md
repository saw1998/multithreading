Exactly! That's a good observation. The difference is the **type of reference** they take.

| Constructor      | Parameter  | Purpose                                                          |
| ---------------- | ---------- | ---------------------------------------------------------------- |
| Copy constructor | `const T&` | Copy from an existing object                                     |
| Move constructor | `T&&`      | Transfer resources from a temporary (or explicitly moved) object |

For example:

```cpp
class ReadLock {
public:
    // Copy constructor
    ReadLock(const ReadLock&) = delete;

    // Move constructor
    ReadLock(ReadLock&&) noexcept = default;
};
```

## Why one `&` vs two `&&`?

### Copy constructor (`const T&`)

```cpp
ReadLock(const ReadLock& other);
```

* `&` means **lvalue reference**.
* It binds to an existing, named object.
* `const` means the source object won't be modified.

Example:

```cpp
ReadLock lock1(rw);

// Would call the copy constructor if it weren't deleted
ReadLock lock2(lock1);
```

Here, `lock1` is an **lvalue** (it has a name).

---

### Move constructor (`T&&`)

```cpp
ReadLock(ReadLock&& other);
```

* `&&` means **rvalue reference**.
* It binds to a temporary object or an object explicitly cast with `std::move`.
* The move constructor is allowed to modify the source object because its resources are being transferred.

Example:

```cpp
ReadLock lock2(std::move(lock1));
```

Now `lock1` can be left in a valid but unspecified (often "empty") state.

---

## Visualizing it

### Copy

```text
lock1  -------- owns resource

Copy

lock2  -------- also owns resource  ❌ Problem for unique ownership
```

Both objects think they own the same resource.

---

### Move

```text
Before move:

lock1 -------- owns resource

After move:

lock1 -------- empty
lock2 -------- owns resource
```

Ownership is transferred, not duplicated.

---

## Why is `std::move` needed?

Even if an object is about to go out of scope, a named variable is still an **lvalue**:

```cpp
ReadLock lock1(rw);

ReadLock lock2(lock1);          // Copy constructor (or compile error if deleted)
ReadLock lock3(std::move(lock1)); // Move constructor
```

`std::move(lock1)` doesn't actually move anything by itself. It simply casts `lock1` to an rvalue reference (`ReadLock&&`), allowing the move constructor or move assignment operator to be selected.

---

## Interview tip: `&&` doesn't always mean "move"

This is an important nuance.

In a declaration like:

```cpp
ReadLock(ReadLock&& other);
```

`&&` is an **rvalue reference**.

However, in a template like:

```cpp
template<typename T>
void func(T&& value);
```

`T&&` is a **forwarding (universal) reference**, which behaves differently because of template type deduction.

Interviewers often ask about this distinction when discussing move semantics and perfect forwarding.
