# Concurrent LLM Serving: Batching Dispatcher

## ELI5: a food truck that only cooks in batches

A food truck's grill is far more efficient cooking six burgers at once than cooking one,
waiting, cooking the next one, waiting again. So the truck collects a handful of orders,
fires the whole batch at once, and then, this is the part that actually matters, hands
each customer back exactly *their own* burger, not a random one from the batch tray.
Customer 3's order was "no onions"; customer 3 had better not get customer 5's burger by
accident just because they were cooked together.

## What you're actually building

Many client threads submit inference requests. A GPU model runs far more efficiently on
a *batch* of prompts at once than one at a time, so requests get collected and sent
through together, but each caller still needs back exactly their own answer.

```cpp
struct Request { int value; };
struct Response { int value; };

class BatchDispatcher {
public:
    explicit BatchDispatcher(
    std::function<std::vector<Response>(const std::vector<Request>&)> process_batch);

    std::future<Response> submit(Request req); // called from many threads at once
    void flush(); // runs whatever's pending as one batch
    std::size_t pending_count() const;
};
```

## Requirements

1. `submit` is called concurrently from many client threads. Every request must be
 safely queued, none lost, none corrupting another request's data.
2. `flush()` runs `process_batch` once on everything currently queued, then resolves
 each caller's own future with their own response, response `i` in the vector
 `process_batch` returns corresponds to request `i` in the vector it was given, in the
 same order.
3. **A caller's `future<Response>::get()` must return exactly the response for *their
 own* request**, never mixed up with another caller's, nobody goes home with the wrong
 burger.

## Why the constraints exist

**`process_batch` may be slow** (it's standing in for a real model's forward pass), it
should only ever be called with the whole pending batch at once, not per-request. That's
the entire point of batching: one trip to the grill, not one per customer.
