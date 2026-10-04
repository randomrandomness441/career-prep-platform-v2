## 1. Reframe

"Don't use exceptions for control flow" is real advice with a specific, mechanical
reason behind it: stack capture, not the throw/catch mechanism, is where the cost lives,
and that cost is proportional to how deep the call stack is when the exception is built.

## 2. What was actually measured, for real

JDK 21, this machine, 20 million iterations with roughly 1 in 7 throwing and
immediately catching:

```
normal exception (captures stack):  1359ms
fillInStackTrace overridden away:    137ms
```

About 10x. The only code difference is one override that turns stack capture into a
no-op.

## 3. The broken version, first

A method that uses exceptions for frequent, expected internal signaling (a common
pattern in some parsers and search algorithms — "stop, I found what I needed") pays the
full stack-capture cost on every single signal, even though nobody will ever look at
that stack trace. The mistake isn't using an exception for control flow per se — it's
paying for a diagnostic feature (the trace) that this specific use case never consumes.

## 4. Interview follow-ups

- Does this cost scale with how deep the call stack is at the point of construction?
  Yes — `fillInStackTrace()` walks every frame currently on the stack, so an exception
  constructed deep inside a long call chain costs more to build than one constructed
  near the top, independent of anything about the exception's own type or message.
- Is overriding `fillInStackTrace()` the only way to reduce this cost? No — some JVMs
  and some exception patterns use a shared, pre-allocated "flyweight" exception instance
  reused across many throw sites specifically to pay the stack-capture cost once at
  startup rather than never or repeatedly; this trades away *any* stack information
  (even for genuine failures) in exchange for zero per-throw cost, a more aggressive
  version of the same tradeoff this question's technique makes.
