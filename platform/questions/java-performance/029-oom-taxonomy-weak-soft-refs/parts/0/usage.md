Plain text, three numbered points. Example shape:

```
1. "Unable to create new native thread" comes from the OS refusing a new
   OS-level thread -- an OS limit or resource exhaustion, unrelated to the
   managed Java heap. More -Xmx changes nothing. Likely real cause: too
   many threads created relative to what the OS allows, often a thread leak.
2. Not broken -- a SoftReference only clears under actual memory pressure,
   so holding everything for days with plenty of free RAM is the documented,
   intended behavior. Worth worrying only if it keeps growing unbounded even
   as the JVM genuinely runs low on memory and still won't clear entries --
   that would mean something else holds a strong reference too.
3. WeakReference clears the instant nothing else holds a strong reference,
   with no regard for actual memory pressure -- for a cache that means
   entries can vanish almost immediately after insertion, making it far
   less effective than intended, not a memory-pressure eviction policy at all.
```
