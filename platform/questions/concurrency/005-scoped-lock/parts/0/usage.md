### How it's called

```cpp
Account alice(100), bob(50);

std::thread t1([&]{ for (int i = 0; i < 1000; ++i) transfer(alice, bob, 1); });
std::thread t2([&]{ for (int i = 0; i < 1000; ++i) transfer(bob, alice, 1); });
t1.join();
t2.join();
// alice.balance() + bob.balance() must still equal 150 -- nothing created or destroyed

transfer(alice, alice, 10);   // same account in and out -- must not hang, must return true
```

Threads call `transfer` on overlapping pairs of accounts, in both directions, at the same
time -- that's exactly the interleaving that deadlocks the naive lock-both version.
