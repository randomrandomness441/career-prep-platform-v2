`std::thread` copy-constructs each argument into its own storage. `unique_ptr` has no copy
constructor — that is the entire point of it. But it does have something else.
---
`std::move(job)` casts it to an rvalue, which selects the *move* constructor instead of the
copy constructor. The pointer transfers and the original becomes null.
---
```cpp
return std::thread([](std::unique_ptr<Job> j, std::atomic<int>* o) {
    o->store(j->id);
}, std::move(job), out);
```
The lambda takes the `unique_ptr` **by value**, so it owns it and destroys it when done.
