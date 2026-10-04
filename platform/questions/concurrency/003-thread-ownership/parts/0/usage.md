### How it's called

```cpp
{
    joining_thread jt([]{ do_work(); });
}   // destructor runs here -- must join before the scope exits, no explicit join() call

std::vector<joining_thread> pool;
for (int i = 0; i < 8; ++i)
    pool.emplace_back([]{ do_work(); });
// pool's destructor joins all 8 when it goes out of scope

joining_thread a([]{ do_work(); });
joining_thread b([]{ do_work(); });
a = std::move(b);   // must join a's original thread before taking over b's

a = std::move(a);   // self-move must not blow up
```
