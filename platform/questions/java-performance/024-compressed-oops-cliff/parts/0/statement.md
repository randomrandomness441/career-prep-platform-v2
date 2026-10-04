## ELI5: the phone directory that switches to full numbers for everyone

A company gives every employee a short, 4-digit internal extension instead of their
full phone number, because with fewer than 10,000 employees, 4 digits is always enough
to uniquely identify anyone — every directory entry, every internal reference, stays
short. The day the company hires its 10,001st employee, 4 digits stop being enough for
*anyone* — not just the new hire. Every single directory entry in the whole company,
including employee #1, has to switch to the longer format, because the system can no
longer guarantee 4 digits are sufficient for a lookup.

The JVM does something structurally similar with object references. On a 64-bit JVM, a
plain object reference is normally 8 bytes. Below a certain heap size, the JVM instead
uses **compressed oops**: a 32-bit (4-byte) reference that gets shifted to address a
much larger range than 4GB would normally allow (by default, an 8-byte-aligned address
space, since every object starts on an 8-byte boundary). Cross the heap size where that
trick stops working, and *every* reference in the JVM — not just to new objects, all of
them — goes back to full 8-byte size.

## What you're actually building (understanding)

Real, measured on this machine, JDK 21, testing exactly where `-XX:+UseCompressedOops`
flips from ergonomically-enabled to disabled:

```
-Xmx32700m:  UseCompressedOops = true
-Xmx32710m:  UseCompressedOops = true
-Xmx32720m:  UseCompressedOops = true
-Xmx32730m:  UseCompressedOops = true
-Xmx32740m:  UseCompressedOops = false
-Xmx32750m:  UseCompressedOops = false
```

The cutoff is between 32,730MB and 32,740MB — close to, but measurably **under**, a
naive "32GB" (32,768MB). The commonly-cited "32GB" folklore number is an approximation;
the actual ergonomic cutoff leaves a small safety margin below the theoretical maximum.

Isolating the size effect directly (same heap size, `UseCompressedOops` forced on vs.
off explicitly, allocating 3 million identical objects with two reference fields):

```
compressed oops ON:   37.7 bytes/object
compressed oops OFF:  51.2 bytes/object
```

About **36% more memory per object** with compressed oops off, for identical objects.

## Requirements

1. Why is the real cutoff *below* the naive 32GB theoretical maximum, rather than
   exactly at it? Reason from the fact that a heap needs room for more than one
   object's worth of address space at the boundary.
2. A service currently runs with a 30GB heap and starts hitting memory pressure. An
   engineer's first instinct is "just bump it to 40GB, more headroom is always safer."
   Given everything above, what real, non-obvious risk does that specific change
   introduce, that a smaller increase (say, to 31GB) wouldn't?
3. The 36% per-object measurement was for a small object with exactly two reference
   fields. Would you expect this percentage to be roughly the same, bigger, or smaller
   for an object with zero reference fields (just primitives) versus an object with many
   reference fields? Reason about what's actually growing when oops go uncompressed.

## Why this matters

"Bigger heap is always safer" is a reasonable-sounding default that has a real, sharp
counterexample built into the JVM itself — crossing one specific size threshold can make
every object in the entire heap bigger, which can mean *more* GC pressure from a
*larger* heap, not less.
