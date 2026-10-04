## How do virtual functions work?

Every object of a class with at least one `virtual` function carries a hidden pointer,
usually called the **vptr**, pointing at that class's **vtable** (virtual table): an
array of function pointers, one slot per virtual function, filled in with whichever
override is correct for that exact class.

```cpp
struct Animal {
    virtual void speak() { /* ... */ }
    virtual ~Animal() = default;
};
struct Dog : Animal {
    void speak() override { /* ... */ }
};
```

Every `Dog` object's vptr points at `Dog`'s vtable, where the `speak` slot holds
`Dog::speak`'s address, not `Animal::speak`'s, even though the pointer's *static* type
might be `Animal*`. Calling `animal_ptr->speak()` compiles to, roughly, "load the vptr
from the object, load the `speak` slot from the vtable it points to, call that address"
, a level of indirection that happens at runtime, which is exactly what lets the same
line of calling code reach a different function depending on the object's actual type,
decided by which vtable its vptr happens to point at.

Non-virtual functions skip all of this, they're resolved at compile time from the
pointer's *static* type, which is why calling a non-virtual function through a base
pointer always runs the base class's version, no matter what the object's real type is.
That's the entire practical reason to mark a function `virtual` in the first place: it's
opting into this runtime lookup instead of the default compile-time one.

## Memory layout for multiple base classes

A class inheriting from two base classes lays each base out as a contiguous block
inside the derived object, one after another, followed by anything the derived class
adds itself:

```
[ Base1's members (incl. its own vptr, if it has virtuals) ]
[ Base2's members (incl. its own vptr, if it has virtuals) ]
[ Derived's own members ]
```

The consequence worth stating explicitly, because it surprises people the first time
they hit it: **a `Derived*` and a `Base2*` pointing at the same object are not
necessarily the same address.** `Base2`'s block starts at some non-zero offset into the
object, so converting a `Derived*` to a `Base2*` silently adjusts the pointer value by
that offset (the compiler inserts this adjustment automatically at every such
conversion; you never write it yourself, but it's really happening). If `Base1` and
`Base2` both declare a function with the same name, the derived class needs two separate
vtables, one for use through a `Base1*`, one for use through a `Base2*`, because a
call through each pointer type has to land in the right base's version of the slot,
adjusted from a different starting offset.

This is also the mechanical reason **virtual inheritance** exists: without it, a
"diamond" hierarchy (two bases that both derive from a common grandparent) gets two
separate copies of the grandparent's data laid out inside the derived object, one
inside each base's block, which is usually not what anyone wants. Virtual inheritance
changes the layout so there's exactly one shared grandparent block, referenced by both
bases through an extra layer of indirection, at the cost of a slightly more complex
(and slightly slower) memory layout.
