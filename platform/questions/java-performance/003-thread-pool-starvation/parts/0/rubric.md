A good answer covers:

- **Starvation, not exhaustion.** There are threads — 20 of them, all present. They're
  not free to do new work because they're stuck waiting on a slow downstream call, not
  because the pool ran out of headcount. The thread dump is the actual evidence: `WAITING
  (parking)` inside a downstream client call, not "no thread available to accept a new
  request." A good answer names this distinction explicitly, not just relabels the
  symptom.
- **A CPU flame graph would show almost nothing here** — low CPU usage across the board,
  because the threads aren't computing, they're parked waiting on I/O. The right tool is
  off-CPU analysis (or, more directly available here, the thread dump itself already
  shown) — something that captures *why* a thread isn't running, not *what* it's doing
  while it runs. This is a direct real-world instance of the base pack's off-CPU lesson:
  CPU usage staying low while latency explodes is the off-CPU signature.
- **Why more threads makes it worse.** More threads means more concurrent requests can
  get stuck waiting on the same slow pricing service at once, which increases load on
  that already-struggling downstream service and can push it (and everything depending
  on it) further into failure — the bottleneck isn't thread count, it's a slow
  dependency, and adding threads just lets more requests pile up behind the same
  underlying wait instead of fixing it. A stronger answer also names a real fix: a
  timeout and circuit breaker on the pricing call, so a slow downstream fails fast
  instead of holding a thread indefinitely.

NEEDS_WORK if the answer calls this exhaustion, or proposes adding threads without
naming why that doesn't address the root cause.

## Code

**Inefficient — blocking call with no timeout, holds a thread indefinitely:**
```java
void process(Order order) {
    double price = pricingClient.getPrice(order.sku());   // can block forever
    order.setPrice(price);
}
```

**Correct — bounded wait, fails fast instead of parking the thread:**
```java
void process(Order order) {
    double price = pricingClient.getPriceAsync(order.sku())
        .orTimeout(500, TimeUnit.MILLISECONDS)
        .join();
    order.setPrice(price);
}
```

**Alternative — a circuit breaker, so a struggling downstream stops receiving new load entirely:**
```java
CircuitBreaker breaker = CircuitBreaker.ofDefaults("pricing");

void process(Order order) {
    double price = breaker.executeSupplier(() -> pricingClient.getPrice(order.sku()));
    order.setPrice(price);
}
```

