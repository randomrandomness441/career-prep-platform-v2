# How Do Virtual Functions Work?

## ELI5: a personalized phone list instead of one shared instruction sheet

Imagine every type of object carries its own personal phone list of "who to call when
something happens", and calling `shout()` on an object doesn't hard-wire which
specific `shout` function runs; it means "look up 'shout' on *this object's* phone
list, and call whoever's listed there." A `Dog` and a `Cat` both have a `shout` entry,
but they each point to a different actual function, so the exact same line of calling
code produces different behavior depending on which object it's called on, decided at
the moment of the call, not baked in when the code was written.

## The question

**How do virtual functions actually work under the hood, what makes `d->shout()`
call the right override, at runtime, when the compiler doesn't know `d`'s real type?**

A fundamentals question reported directly from a Pure Storage interview. The strong
answer names the mechanism (a vtable) precisely, not just "the compiler figures it
out."
