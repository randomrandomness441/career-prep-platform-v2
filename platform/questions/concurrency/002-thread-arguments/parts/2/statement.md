# Follow-up 2, the one-of-a-kind package

## ELI5: some things can't be photocopied

A sealed, one-of-a-kind package, say, the only key to a safe, can't be handed to two
people at once. You either keep it, or you give it away completely. There's no
in-between "here's a copy, we both have one now."

`std::unique_ptr` is that kind of package: exactly one owner, ever, and handing it off
means you no longer have it. A `std::thread`'s normal way of taking arguments is to
*copy* them for the new thread to use, which works fine for things you can photocopy,
but a `unique_ptr` refuses to be copied. That refusal is the compiler error you're about
to hit, and it's worth actually reading rather than pattern-matching past.

## What you're actually building

```cpp
struct Job { int id; };
std::thread run_job(std::unique_ptr<Job> job, std::atomic<int>* out);
```

The returned thread must write `job->id` into `*out`. Once the thread is launched, the
thread **owns** the job completely. Nothing outside may touch it afterwards, the same
way nobody but the safe's new owner gets to use the key.

## Why the constraints exist

- **Keep the signature. Don't switch to `shared_ptr`.** Swapping in a `shared_ptr` would
 dodge the actual lesson here. This question is about *transferring* sole ownership
 into the thread, not sharing it.
- **After launching, the local `job` must be empty.** That's the proof the handoff was
 real: ownership moved, it didn't get duplicated. If you can still use `job` after
 `run_job` starts the thread, something's wrong. There'd be two "owners" of one key.
