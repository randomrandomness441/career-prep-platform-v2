# Passing Arguments to Threads

## ELI5: the helper who adds coins to your jar

You have a jar of coins. You ask a helper: "go add 5 coins to *my* jar." If the helper
accidentally works on a photocopy of your jar instead of the real one, they'll happily
report "done!" Your actual jar is untouched. That's the whole bug in this question. A
thread is like a helper you send off to do a job. If you're not careful about *which*
jar you hand them, the real one or an accidental copy, the work vanishes into thin air.

## What you're actually building

Two functions must each start a thread (send off a helper) that adds `n` coins to a
`Counter` **you** own, then wait for the helper to finish before you check the jar:

```cpp
struct Counter { int value = 0; };
void add_n(Counter& c, int n);          // given: adds n to c.value

void launch_lambda(Counter& c, int n);    // start a thread using a lambda
void launch_function(Counter& c, int n);  // start a thread using add_n directly
```

Both currently fail, in different ways. `launch_lambda` compiles and silently does
nothing to your counter, the photocopy problem above. `launch_function` is the version
you have to write from scratch, and the obvious way to spell it doesn't even compile.

After each returns, your `Counter` must hold `n`.

## Why the constraints exist

- **Join before returning, no detached threads.** You have to actually wait for the
  helper to finish and come back before you're allowed to look in the jar. Otherwise
  you might peek before they're done.
- **Don't change `Counter` or `add_n`.** The bug is entirely in how you hand the jar to
  the helper, not in the jar or the adding logic themselves.
- **Don't add a mutex.** There's no *shared*-data problem here. Nobody else is touching
  the jar at the same time you are. This is purely a lifetime/ownership question: is the
  helper working on your real jar, or an accidental copy of it?
