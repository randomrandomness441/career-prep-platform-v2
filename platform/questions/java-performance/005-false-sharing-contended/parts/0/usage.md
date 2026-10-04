Plain text, three numbered points. Example shape:

```
1. There's no race to fix -- each field is only written by its own thread. A
   lock adds overhead without touching the actual cause, which is a hardware
   cache effect from physical proximity, not unsafe shared mutation.
2. It would show both threads' increments as normal, cheap-looking code --
   the real cost is cache-coherency stalls between cores, invisible to a
   call-stack sampler. Hardware cache-miss counters would actually reveal it.
3. Related but different mechanisms serving different purposes. LongAdder
   stripes the count across cells so threads mostly write to different
   memory (fixes true contention), but the cells themselves are padded the
   same way @Contended would do it, to stop the stripes from false-sharing
   each other.
```
