*Demo question — proves the `judged` question kind works end to end. Not a vetted content
pack; replace or delete this folder once you've seen the plumbing work.*

## The problem

You're building a rate limiter for a public API: each client gets at most N requests per
minute. Design it for a service running on multiple stateless nodes behind a load balancer,
so no client can cheat by hitting a different node.

Cover, in your answer:

- What state you keep, and where it lives.
- Which algorithm you use (fixed window, sliding window, token bucket, leaky bucket) and why
  that one over the others.
- How multiple nodes agree on one client's count without a single node becoming a bottleneck.
- What happens at the edges: a burst right at a window boundary, the shared store going down,
  a client with a clock skewed from the server's.

There's no code to write here. Answer in plain writing, the way you'd talk it through on a
whiteboard.
