# Two Mutexes Without a Deadlock

## ELI5: the phone call that drops

You're on a call with a friend and the line drops. You both think "I'll be polite and
wait for them to call back." So you wait. And they wait too, thinking the exact same
thing. Nobody calls. You'll both sit there forever unless one of you breaks the
politeness rule and just calls first.

That's a deadlock: two people (or threads), each holding something the other one needs
("I'll wait for you"), and neither willing to go first. Nothing crashes. Nothing prints
an error. Everything just stops, forever, and only in the unlucky case where the
timing lines up exactly wrong.

## Where this shows up: bank transfers

Each `Account` has its own lock, because one giant bank-wide lock would make every
customer on Earth wait behind every other customer for every single transaction. Moving
money touches *two* accounts, so `transfer` has to hold two locks at once, and here's
the version everyone writes first:

```cpp
bool transfer(Account& from, Account& to, long amount) {
    std::lock_guard<std::mutex> g_from(from.m_);
    std::lock_guard<std::mutex> g_to(to.m_);
    if (from.balance_ < amount) return false;
    from.balance_ -= amount;
    to.balance_ += amount;
    return true;
}
```

Now run two threads:

```
thread 1: transfer(alice, bob, 10)   locks alice, then wants bob
thread 2: transfer(bob, alice, 10)   locks bob,   then wants alice
```

Thread 1 is holding alice's lock, waiting for bob's. Thread 2 is holding bob's lock,
waiting for alice's. That's the dropped phone call, exactly. Each one is waiting for
the other to let go first, and neither ever will.

## Your task

Implement `transfer` so that no schedule can deadlock it:

```cpp
class Account {
public:
    explicit Account(long initial);
    Account(const Account&) = delete;
    Account& operator=(const Account&) = delete;

    long balance() const;             // locks

private:
    mutable std::mutex m_;
    long balance_;
    friend bool transfer(Account& from, Account& to, long amount);
};

// Moves `amount` from `from` to `to`.
// Returns false and changes nothing if `from` does not have enough.
bool transfer(Account& from, Account& to, long amount);
```

## Requirements

1. **No deadlock, under any schedule and any argument order.** The tests run threads in
   both directions at once, and over six accounts in random pairings.
2. **Atomic.** The balance check and the two updates happen with both accounts locked, so
   no account can go negative and money is never created or destroyed.
3. **`transfer(a, a, n)` must not hang.** Locking one `std::mutex` twice on the same
   thread is undefined behaviour, and `std::scoped_lock` and `std::lock` both require the
   mutexes you hand them to be distinct. Same account in and out: change nothing, return
   `true`.
4. **No global bank-wide lock.** Two threads transferring between four unrelated accounts
   must be able to proceed at the same time.

## Why the constraints exist

- **One `std::mutex` per `Account`, kept private.** Anything coarser than per-account
  locking defeats the point of the exercise.
- **No `std::recursive_mutex`.** That hides requirement 3 (the same-account case) instead
  of actually solving it, and does nothing at all about requirement 1's deadlock.
- **No sleeping, retry loops, or timeouts of your own devising.** The standard library
  already has the tool for "lock several mutexes without deadlocking," reach for that,
  not a workaround.
