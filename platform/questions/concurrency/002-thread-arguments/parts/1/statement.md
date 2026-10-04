# Follow-up 1, the sticky note that gets thrown away

## ELI5: writing an address on a sticky note that's about to be binned

Imagine you scribble someone's name on a sticky note, stick it to an envelope, and tell
a courier "deliver this once I say go." Then you toss the notepad you wrote it from in
the trash. The envelope still has the sticky note on it, and that's fine, the note is
attached to the thing being delivered. But say instead you'd written the name on the
*notepad itself* and just told the courier "the name is on page 3 of my notepad, go look
it up when you deliver it." Then you threw the notepad away before the courier ever
opened it. The courier shows up to an empty trash can where page 3 used to be.

That second situation is this question. A factory builds a thread (the courier) and
hands it back to you, to send off later:

```cpp
struct Result { std::string text; };

std::thread make_labeler(Result* out, std::binary_semaphore* go, int id);
```

The returned thread must:
1. wait for you to say "go" (release the semaphore),
2. write `"worker-<id>"` into `out->text`.

The label text is built inside `make_labeler` from a **local** string, the "notepad."
The moment `make_labeler` returns, that local variable is gone. But the thread hasn't
run yet, it's still waiting on `go`. When it finally wakes up and reaches for the label,
is the notepad still there, or is it reaching into the trash?

## Your task

Make the thread produce the correct label no matter what you do, or how long you wait,
between `make_labeler` returning and releasing `go`.

## Why the constraints exist

- **Keep the signature.** You (the caller) own `out` and `go`, and both outlive the
 thread, that part's already fine. The bug is specifically about the label text.
- **The thread must still wait on `go` before writing.** You're not allowed to dodge the
 problem by writing the label before the thread even starts.
- **Don't make the label a global or `static`.** That would dodge the real question.
 Fix *who owns the label and for how long*, not where in memory it happens to live.

**Why this one matters:** the buggy version frequently prints the right answer anyway.
The trash just happens to still have the old page sitting on top, unclaimed, when the
courier looks. The test below deliberately overwrites that spot before releasing the
thread, which is what a real program does by accident, under load, at 3am.
