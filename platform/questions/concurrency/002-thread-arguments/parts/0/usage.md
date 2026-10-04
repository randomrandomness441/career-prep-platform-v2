### How it's called

Both functions are called directly by the caller (no threads of your own to spawn beyond
what's inside them) — they must join internally before returning:

```cpp
Counter a;
launch_lambda(a, 1000);     // starts a thread, joins it, returns
// a.value must be 1000 here

Counter b;
launch_function(b, 1000);   // same contract, via add_n instead of a lambda
// b.value must be 1000 here
```

Neither function may return before its internal thread has finished and joined.
