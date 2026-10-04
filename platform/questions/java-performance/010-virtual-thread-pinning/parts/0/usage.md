Plain text, three numbered points. Example shape:

```
1. Virtual-thread count and queue depth don't reveal it -- the constrained
   resource is the small, fixed carrier pool. You can have thousands of
   virtual threads and a healthy-looking queue while the number of carriers
   actually free to run any of them is zero.
2. Enable the jdk.VirtualThreadPinned JFR event (available since JDK 21), or
   run with -Djdk.tracePinnedThreads=full to get a stack trace at the exact
   pin site.
3. ReentrantLock fixes it now but means finding and changing every
   synchronized block on a blocking path across the codebase -- real,
   distributed effort, easy to miss one. Upgrading to JDK 24+ fixes every
   instance at once with no code changes, but means waiting on a JDK upgrade
   with its own migration risk and timeline a team may not control.
```
