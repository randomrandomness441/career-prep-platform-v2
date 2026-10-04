Plain text, three numbered points. Example shape:

```
1. Raise the sampling frequency (e.g. -F 999 for a short targeted capture) so
   the gap between samples shrinks closer to the event's own duration. Or
   switch to tracing/instrumenting that specific function instead of relying
   on sampling to get lucky.
2. Likely cause: missing debug symbols -- a stripped binary or library, or a
   container image built without keeping .debug files. Next step: rebuild
   with symbols available before assuming there's nothing there.
3. Consider inlining -- the compiler or JIT may have folded several distinct
   functions into that one native frame, so the profiler has nothing to break
   down even though real structure exists in the source. Worth checking with
   a diagnostic build that preserves inline-frame info.
```
