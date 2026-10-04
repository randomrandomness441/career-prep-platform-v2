# Keyed Async Executor for a Microservice

## ELI5: a support queue where your own tickets stay in order

A support desk has a handful of agents. Tickets for *different* customers can be handled
by different agents at the same time, no reason customer A's issue should wait on
customer B's. But if customer A files three tickets in a row, those three have to be
handled **in the order A filed them**, you can't let ticket 3 get resolved before ticket
1 just because it happened to land on a faster agent. Same customer, strict order.
Different customers, totally parallel.

## What you're actually building

A common microservice shape: process incoming events off the request path, on a fixed
pool of worker threads, so the caller doesn't wait for the actual processing. The
wrinkle: events that share a key (the same user, the same order, the same account) must
still be processed **in the order they were submitted, relative to each other**, while
events for different keys should run freely in parallel.

```cpp
class KeyedAsyncExecutor {
public:
    explicit KeyedAsyncExecutor(int num_workers);
    ~KeyedAsyncExecutor(); // drains everything first
    void submit(std::string key, std::function<void()> task); // fire-and-forget
};
```

## Requirements

1. Two tasks submitted with the **same** key never run concurrently with each other, and
 run in the order they were submitted, customer A's tickets, in order.
2. Tasks with **different** keys may run concurrently, across the worker pool, different
 customers, different agents, at once.
3. Every submitted task eventually runs, and the destructor doesn't return until
 everything already submitted has completed.

## Why the constraints exist

**A fixed pool of `num_workers` threads, not one thread per key.** The number of
distinct keys (customers) can be far larger than the pool (agents); you can't hire a
dedicated agent for every customer who's ever filed a ticket.
