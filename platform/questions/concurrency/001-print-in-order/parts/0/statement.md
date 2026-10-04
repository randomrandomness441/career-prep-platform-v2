# Print in Order

## ELI5: the relay race

Three runners stand at the starting line: Alex, Blake, and Casey. The starter's gun goes
off for all three **at the same time**, but they don't actually run in a random order.
Alex has to run and finish first. Only then can Blake go. Only after Blake finishes can
Casey go. If you filmed the race and it ever showed Blake crossing the line before Alex,
something went wrong.

The tricky part: the three runners are told "go!" simultaneously, by three different
people, in an order nobody controls. Alex might get the signal a split second after Casey
does. It's on *you*, not the starter, to make sure that no matter what order the "go!"
signals arrive in, the actual running still happens Alex, then Blake, then Casey, every
single time. And nobody's allowed to just stand there twitching, checking "is it my turn
yet? is it my turn yet?", that's cheating (and it wastes a lot of energy for nothing).

## What you're actually building

Three threads share one `Foo` object. Thread A calls `first()`, thread B calls
`second()`, thread C calls `third()`, started in whatever order the scheduler feels
like. Each method is handed a callback (the "run!" action) to invoke exactly once, in
the right order:

```cpp
void first(std::function<void()> printFirst);
void second(std::function<void()> printSecond);
void third(std::function<void()> printThird);
```

Your job: make the output **always** read `firstsecondthird`, no matter which thread's
`go!` arrives first.

## Why the constraints exist

- `printFirst()` must fully finish before `printSecond()` starts, same for second before
 third. That's the whole race: no overlap, no skipping ahead.
- No busy-waiting. A thread that isn't allowed to run yet has to actually go to sleep,
 not spin in a loop repeatedly asking "my turn yet?", that's real CPU burned for zero
 useful work while it waits.
