## ELI5: the relay runner who starts before the baton arrives

A relay racer doesn't wait for the baton to physically touch their hand before starting
to run — they start accelerating a fraction of a second early, betting the handoff will
go the way it usually does, because waiting for confirmation first would waste real
time. If the bet is right, they gain a real head start, every time. If the bet is
wrong — the handoff happens differently than expected — they have to stop, throw away
the wasted motion, and start over correctly. That correction costs more time than if
they'd just waited in the first place.

A CPU does exactly this with `if` statements, constantly, thousands of times a second.
This is called **branch prediction**. Before it even knows which way an `if` will go, it
guesses (based on that branch's recent history) and starts executing down the guessed
path. Guess right, and the guess was free — actual speed gained. Guess wrong, and the
CPU has to discard all that speculative work and restart down the correct path, which
costs real, measurable time — a **misprediction penalty**.

## What you're actually building (understanding)

Real numbers, measured on this machine, JDK 21 — a loop checking a boolean "guard" per
iteration (a common shape: `if (level.isEnabled()) { doExpensiveWork(); }`), comparing
patterns with roughly the same amount of *actual work done*, differing only in how
predictable the pattern of true/false is:

```
long, predictable runs of true/false (switches every 10,000): 184ms
truly random true/false, same 50/50 split:                    397ms
```

Both patterns do roughly the same total amount of real work — about half the iterations
take the expensive path, half take the cheap one. The random pattern takes **more than
twice as long**, purely because the CPU can't reliably predict which way the check will
go this time, and pays a real, repeated cost for guessing wrong.

## Requirements

1. Why does a genuinely 50/50 random pattern cost more than a predictable pattern doing
   the exact same *amount* of work? What's actually different between the two, given
   the CPU doesn't know the future in either case?
2. A real, published benchmark of a logging library found something even more dramatic:
   when a log level is *consistently* disabled across an entire benchmark run, a design
   using a polymorphic "guard object" (swapped only when configuration changes) beat a
   plain `if (isEnabled())` check by roughly 500x in that specific scenario. A
   perfectly, consistently predicted `if` (always false, every single time) should
   already be nearly free for the CPU's branch predictor — so a 500x gap is too large to
   explain by misprediction avoidance alone. What's the *other* real mechanism likely
   doing most of the work here, given that the polymorphic design routes disabled calls
   to a literal no-op object? (Think about what a naive `if (isEnabled()) { log("x=" +
   expensiveToString(x)); }` does with its arguments *before* the check ever runs.)
3. Would the polymorphic-guard technique from requirement 2 help in a scenario where the
   *guard's own value* changes unpredictably on every single call (not the log-level
   case — imagine something that's enabled and disabled every few calls, unpredictably,
   in production)? Why or why not, tying back to question 018's specific finding about
   what breaks a call site's fast dispatch path.

## Why this matters

"Avoid branches for performance" is usually stated as a low-level bit-tricks technique.
The real, more valuable version of this lesson is architectural: moving a decision from
"checked on every call, unpredictably" to "decided rarely, then dispatched through a
stable reference" can eliminate the misprediction cost entirely, without touching a
single CPU instruction by hand.
