## ELI5: the couch that doesn't fit through any normal door

A moving company's storage warehouse is divided into evenly-sized rooms. Furniture
normally goes into whichever room has space, no special handling. Then someone tries to
store a couch too big to fit through any single room's door frame — it has to be broken
down, or the movers have to knock through a wall into the next room just to fit it.
Either way, that couch doesn't follow the normal "any room with space" rule, and it
tends to leave awkward, hard-to-fill gaps around it.

G1 divides the heap into equal-sized regions (the "rooms"). Any single object larger
than **half a region's size** is classified as **humongous**, skips the normal young
generation entirely, and gets placed directly into one or more contiguous old-generation
regions reserved just for it — the equivalent of knocking through walls to fit the
couch.

## What you're actually building (understanding)

Real value from this machine's default G1 ergonomics (JDK 21, default heap sizing):

```
G1HeapRegionSize = 2097152 bytes  (2MB)
```

That means, on this exact configuration, any single object over **1MB** (half of 2MB)
is humongous. Region size itself is chosen ergonomically based on heap size — a service
with a much larger max heap would likely get a larger region size, and therefore a
higher humongous threshold, without anyone setting anything explicitly.

## Requirements

1. A service allocates large `byte[]` buffers (say, 1.5MB each) frequently, and each one
   is genuinely short-lived — used for one request, then immediately eligible for
   collection. Given the humongous threshold on this machine, would these buffers
   bypass the young generation? What does that imply for how "expensive" a supposedly
   short-lived object actually is here, compared to a normal short-lived object under
   1MB?
2. Why does a steady stream of humongous allocations tend to cause heap fragmentation
   specifically, in a way that many small short-lived objects normally don't?
3. Given that region size is chosen ergonomically from heap size, name one real,
   concrete lever available to fix a humongous-allocation problem without changing a
   single line of application code.

## Why this matters

"Short-lived objects are cheap in a generational collector" is true right up until an
object crosses this specific size threshold, at which point the entire premise that
makes generational collection fast (young objects die young generation, cheaply) stops
applying to it — a size-triggered exception to a rule most Java engineers treat as
universal.
