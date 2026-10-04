### How it's called

The caller builds the thread, does other work (which deliberately overwrites the stack
space `make_labeler`'s locals used to occupy), then releases `go` and joins:

```cpp
Result r;
std::binary_semaphore go{0};

std::thread t = make_labeler(&r, &go, 7);   // thread parks on `go` immediately

do_unrelated_stack_heavy_work();            // stomps on make_labeler's old stack frame

go.release();                                // now let the thread run
t.join();
// r.text must be "worker-7"
```
