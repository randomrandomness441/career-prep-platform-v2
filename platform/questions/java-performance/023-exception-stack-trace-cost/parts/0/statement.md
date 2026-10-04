## ELI5: the accident report nobody reads

Every time a delivery driver takes a wrong turn, company policy says they have to pull
over, write a full incident report documenting exactly how they got there, step by
step, back to when they left the warehouse — even if they immediately notice the wrong
turn and correct it themselves without needing anyone's help. Writing that report takes
real time, every single time, whether or not anyone ever reads it.

Throwing a Java exception works like this. The expensive part usually isn't the
`throw` or the `catch` — it's that creating the exception object captures a full
snapshot of the call stack, by default, every time, whether or not anything ever
inspects it.

## What you're actually building (understanding)

Real, measured on this machine, JDK 21 — a loop where about 1 in 7 iterations throws
and immediately catches an exception, 20 million iterations:

```
normal exception (captures stack):  1359ms
fillInStackTrace overridden away:    137ms
```

About **10x** slower with the normal, default stack-capture behavior. The only
difference between the two exception classes: one overrides `fillInStackTrace()` to do
nothing and return itself instead of walking the stack:

```java
class NoStackException extends RuntimeException {
    NoStackException(String m) { super(m); }
    @Override public synchronized Throwable fillInStackTrace() { return this; }
}
```

## Requirements

1. Why does capturing a stack trace cost real, measurable time, given that it happens
   before the exception is ever thrown or caught? What is it actually doing?
2. A method uses an exception purely for internal control flow — signaling "stop
   searching, found it" by throwing and immediately catching within the same method,
   never crossing a real error boundary, never logged, never shown to a user. Is
   overriding `fillInStackTrace()` a reasonable choice here? What would make it a bad
   idea to apply the same override to an exception thrown from, say, a public API
   boundary?
3. This cost exists per-exception-object, not per-throw. If the *same* pre-constructed
   exception instance is thrown repeatedly (a common trick for "expected, frequent"
   control-flow signals), does it pay the stack-capture cost every time, or only once?

## Why this matters

"Exceptions are for exceptional cases, don't use them for control flow" is usually
taught as a style rule. The real, measurable reason behind it is specific and
mechanical: it's not that throwing is slow, it's that stack capture is slow, and that
cost is avoidable in the narrow, honest case where nobody will ever need the trace.
