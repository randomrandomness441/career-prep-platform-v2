# Follow-up 1, the runner trips

## ELI5: the relay race, continued

Your relay works. Now imagine Alex trips and falls before crossing the line, Alex's leg
of the race never actually finishes. What happens to Blake and Casey, standing there
waiting for a baton hand-off that's never coming?

In the semaphore solution from part 0, this is exactly what happens: `printFirst()`
throws before it gets the chance to signal "go" to `second()`. The signal never comes.
Blake sleeps forever. So does Casey, behind Blake. One runner tripping has permanently
frozen the other two, the whole program hangs and never exits.

```cpp
foo.first([]{ throw std::runtime_error("printer offline"); });
```

## Your task

Make the object survive a throwing callback:

1. The exception must still propagate out of `first()` to its caller, you're not
 allowed to swallow it and pretend nothing happened.
2. `second()` and `third()` must not hang. They should be allowed to proceed.
3. Ordering must still hold for whichever calls *do* succeed.

## Why the constraints exist

**The fix must be exception-safe under *any* callback throwing, not just `printFirst`.**
If Blake trips instead of Alex (`printSecond` throws), Casey must not hang either, the
same "runner goes down, race must still end" logic has to work no matter which leg fails.
