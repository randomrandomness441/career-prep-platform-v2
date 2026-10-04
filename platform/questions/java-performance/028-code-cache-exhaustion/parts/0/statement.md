## ELI5: the parking garage for compiled code

Every method the JIT compiles has to be stored somewhere as real machine code, in a
fixed-size region of memory called the **code cache** — like a parking garage with a
fixed number of spots. Once the garage is full, the attendant can't park any more cars.
It doesn't matter how good a driver shows up next, or how badly they need a spot — there
simply isn't one.

A JVM whose code cache fills up doesn't get slightly slower. It stops compiling
**anything new**, permanently, for the rest of that process's life (unless something
frees space) — including methods that haven't even had a chance to get hot yet.

## What you're actually building (understanding)

Real, measured on this machine, JDK 21 — a program with 1,500 distinct hot methods,
run with an artificially tiny code cache:

```
$ java -XX:ReservedCodeCacheSize=3m CodeCacheFill2

[2.803s][warning][codecache] CodeCache is full. Compiler has been disabled.
[2.803s][warning][codecache] Try increasing the code cache size using -XX:ReservedCodeCacheSize=
CodeCache: size=3072Kb used=3047Kb max_used=3068Kb free=24Kb
 total_blobs=2182, nmethods=1885, adapters=206, full_count=1
Compilation: disabled (not enough contiguous free space left)
Exception in thread "main" java.lang.VirtualMachineError: Out of space in CodeCache for adapters
```

At an extremely tiny cache size, this isn't just "the compiler stops helping" — the JVM
actually **crashed** with a real `VirtualMachineError`, a class most Java engineers have
never personally seen thrown. At a slightly larger cache (4MB, versus this JVM's ~240MB
default), the same 1,500-method program completed successfully with **no measurable
timing difference** from the default cache size at all.

## Requirements

1. Why does this failure mode look nothing like a gradual slowdown — either the cache
   has room and everything is normal, or it runs out and something breaks or freezes in
   place? What does that tell you about how to think about sizing the code cache versus
   sizing, say, heap memory?
2. Given that 4MB was enough for this specific 1,500-method program but 3MB caused a
   hard crash, what's the real, practical risk of shipping a production service with a
   manually-reduced `-XX:ReservedCodeCacheSize` based on "it worked fine when I tested
   it"?
3. Why would a codebase with an unusually large number of distinct methods that all
   become hot (a service handling many different request types, each with its own
   code path, or a codebase generated from many small classes) be at real risk of
   hitting this specific ceiling, when a smaller, simpler codebase running on the exact
   same JVM flags would never come close?

## Why this matters

Every other cliff found in this pack degrades performance. This one can outright crash
the process, or freeze it in a permanently un-optimized state — a category of failure
worth knowing exists before a production incident is the first time you learn about it.
