Plain text, three numbered points. Example shape:

```
1. Predictability, not amount of work. A predictable pattern lets the CPU
   guess right almost every time based on recent history, making correct
   guesses free. A truly random pattern defeats that -- roughly half the
   guesses are wrong, and each wrong guess means discarding speculative work
   and restarting, a real cost paid on top of the branch's actual outcome.
2. A perfectly predicted always-false branch should already be nearly free,
   so 500x is too big for branch prediction alone. The real driver is likely
   eager argument evaluation -- a naive if-check still builds the log string
   before checking, while a guard object routing to a no-op skips that real
   work entirely, not just avoiding a mispredicted branch.
3. It wouldn't help. Question 018 showed the fast path needs a call site to
   stay mostly stable (1-2 types); swapping the guard's value every call
   makes that same call site unstable in a different way. The technique's
   win depends on the decision changing rarely -- if it's genuinely
   unpredictable per call, no architecture removes the need to check it
   every time.
```
