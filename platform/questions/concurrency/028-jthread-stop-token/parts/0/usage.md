### How it's called

```cpp
queue_worker w;                 // starts its own background thread immediately

for (long i = 1; i <= 1000; ++i) w.submit(i);   // called from the main thread

// ... worker is processing jobs concurrently in the background ...

w.request_stop();               // safe to call even while the worker is idle and asleep
w.join();                       // returns promptly either way

std::printf("processed=%d sum=%ld\n", w.processed(), w.sum());

{
    queue_worker w2;
    // nothing submitted -- worker goes straight to sleep on the condition variable
}   // destructor alone must stop and join it, without an explicit request_stop() call
```
