# Work-Stealing Thread Pool

## ELI5: line cooks, each with their own ticket rail, but nobody stands idle

A kitchen has several line cooks. Instead of one shared stack of order tickets that every
cook reaches into, with everyone's hand bumping into everyone else's, each cook gets their
own personal ticket rail. New orders mostly land directly on whichever cook is free to take
them, so most of the time nobody's waiting on anyone else to grab a ticket.

But kitchens aren't perfectly even. Sometimes one cook's rail is piled high while another
cook has nothing on theirs. A good kitchen doesn't let that idle cook just stand there. They
walk over and grab a ticket off the busy cook's rail instead, stealing a bit of the backlog
rather than sitting idle while a coworker drowns.

## What you're actually building

```cpp
class WorkStealingPool {
public:
    explicit WorkStealingPool(int num_threads);
    ~WorkStealingPool();                       // must not lose or abandon queued work
    void submit(std::function<void()> task);   // called from any thread
};
```

## Requirements

1. Every task ever `submit`-ted eventually runs, exactly once, including tickets submitted
   right before the kitchen closes, with no delay in between.
2. **The destructor must not crash the process.** It must wait for every cook to actually
   finish their current work before returning. You don't slam the kitchen shut mid-order.
3. Work is distributed across more than one worker thread under normal load. A design where
   every task piles onto one shared queue defeats the entire point.
4. **An idle worker with no work of its own may steal a task from another worker's queue**
   rather than sit idle while other workers are still busy.

## Why the constraints exist

**One queue per worker thread, not one shared queue for the whole pool.** That's the whole
point of "work-stealing" versus a plain thread pool: most of the time, cooks never even
touch each other's rails, and contention only happens on the rare occasion someone needs to
steal.
