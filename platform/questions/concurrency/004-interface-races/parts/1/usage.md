### How it's called

```cpp
threadsafe_stack<Fragile> s;   // Fragile's copy/move constructor throws on a schedule
s.push(Fragile{...});

// form 1: shared_ptr return
if (std::shared_ptr<Fragile> v = s.pop()) {
    use(*v);
}

// form 2: out-parameter
Fragile out;
if (s.pop(out)) {
    use(out);
}
```

If the copy/move inside either `pop()` throws, the exception may propagate to the
caller — but the element must still be on the stack afterwards, so a retry with the same
call succeeds instead of finding the element gone.
