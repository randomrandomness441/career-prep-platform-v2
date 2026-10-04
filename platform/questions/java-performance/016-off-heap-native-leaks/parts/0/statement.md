## ELI5: the storage unit the inventory system doesn't know exists

A warehouse's inventory system tracks every item on every shelf inside the building
perfectly. It has zero visibility into a separate storage unit the facilities team
rents down the street to overflow excess pallets — that unit isn't part of the
warehouse's system at all, so nothing there ever shows up in an inventory report, no
matter how full it gets or how long forgotten pallets sit in it.

A Java heap dump shows you every object living inside the JVM's managed heap,
perfectly. It has zero visibility into **off-heap** memory: `DirectByteBuffer`
allocations, the JIT's code cache, thread stacks, and anything allocated by native code
through JNI. That memory can grow, leak, and eventually crash the process with an
out-of-memory error, while a heap dump — the first tool most people reach for — shows
nothing wrong at all.

## What you're actually building (understanding)

Two real, distinct blind spots, not one:

- **`DirectByteBuffer` leaks.** A `DirectByteBuffer` is a small Java object living on
  the heap, but it holds a handle to native memory equal to its capacity. The native
  memory is only released when the `DirectByteBuffer` object itself is garbage
  collected (or explicitly freed) — if references to these buffers are held longer than
  intended (cached, pooled incorrectly, or simply never released), the *Java object*
  might look small and harmless in a heap dump while the *native memory it points to*
  keeps growing.
- **Raw JNI leaks are worse.** Native Memory Tracking (NMT), the JVM's own tool for
  watching non-heap memory, only tracks allocations the *JVM itself* makes. A native
  library called through JNI that does its own `malloc` calls directly is invisible to
  NMT too — nothing built into the JVM sees it at all.

## Requirements

1. A service's heap dump looks completely normal — no unusual object counts, no obvious
   retained references, heap usage stable. RSS (resident set size, the process's actual
   real memory usage from the OS's point of view) keeps climbing anyway until the
   container gets OOM-killed. What's the first category of memory you'd suspect, given
   the heap dump already ruled out normal heap growth?
2. Between a `DirectByteBuffer` leak and a raw JNI `malloc` leak from a native library,
   which one would Native Memory Tracking actually help diagnose, and which one would
   it miss entirely? Why the difference?
3. Name one concrete tool or technique for finding the JNI-`malloc` case specifically,
   given that NMT can't see it.

## Why this matters

"The heap dump looks fine" is not the same claim as "there's no leak." For a growing
share of real Java services — anything using NIO buffers heavily, or wrapping a native
library — the actual leak lives in exactly the place the most commonly reached-for tool
cannot see at all.
