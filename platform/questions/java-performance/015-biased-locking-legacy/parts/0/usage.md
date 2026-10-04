Plain text, three numbered points. Example shape:

```
1. Revocation needs the JVM's safepoint mechanism, which is a whole-JVM
   coordination point by design -- there's no narrower "freeze just these
   two threads" primitive for it, so it reuses the same global mechanism GC
   uses for an unrelated reason.
2. A pause with no correlating GC log entry at all, on an older JDK, is the
   specific tell -- it rules out question 004's GC pauses (which always show
   in GC logs) and question 007's throttling (which correlates with CPU
   quota metrics, not lock handoff). "Pause with nothing else explaining it"
   plus lock objects starting to be touched by a second thread is the shape.
3. What JDK version is actually running. If it's 15+ (deprecated) or 21+
   (off by default), this cause is very unlikely or impossible, and time is
   better spent elsewhere -- checking the version should be the first step,
   not something checked after already chasing this theory.
```
