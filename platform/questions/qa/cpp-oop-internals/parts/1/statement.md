# Memory Layout for a Class With Multiple Base Classes

## ELI5: stacking two separate toolboxes, then adding your own tray on top

A class that inherits from two base classes is like taking two separate, pre-packed
toolboxes and physically stacking one on top of the other, then adding your own tray on
top of both. Each original toolbox keeps its own internal layout completely intact,
nothing inside either one gets rearranged. What changes is that anyone holding a
pointer to *just the second toolbox* needs to know how far into the stack it starts,
so they can find their tools without touching the first toolbox's contents by mistake.

## The question

**Given two base classes and a class derived from both, how would you lay out the
derived object's memory so that polymorphism still works correctly through either base
class pointer?** (Simplify: assume memory is a series of same-sized slots, and each
method/attribute takes up one slot.)

Reported directly from a Pure Storage interview. The core insight worth naming: a
`Derived*` and a `Base2*` pointing at the *same* object are not always the same address.
