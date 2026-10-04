# Async Logger, Zero Allocation

## ELI5: a guest sign-in sheet with a fixed number of pre-printed lines

Imagine a sign-in sheet at a busy front desk, pre-printed with exactly, say, 10,000 blank
lines, no more paper gets added, ever. Visitors walking in just grab the next free line
and fill in their name. Nobody stops to run to the supply closet for more paper mid-rush
, that would be slow, and paper's the one thing you can't be waiting on when a hundred
people are trying to sign in at once. Once all 10,000 lines are filled, the sheet is full:
new visitors get turned away (not squeezed onto someone else's line, not given a
half-written line) until someone comes and collects the sheet.

Meanwhile, one person periodically walks up and reads off everything that's been fully
written so far, never a line that's only half-filled-in, mid-signature.

## What you're actually building

Many application threads call `log()` from arbitrary, possibly latency-sensitive code.
One background thread periodically calls `drain()` to collect what's been logged so far
(and, in a real system, writes it out to disk or a socket, this exercise stops at
collecting it correctly).

```cpp
struct LogRecord {
    int producer_id;
    long seq;
    char message[48];
};

class Logger {
public:
    explicit Logger(std::size_t capacity);
    bool log(int producer_id, long seq, const char* msg); // never allocates
    std::vector<LogRecord> drain();
};
```

## Requirements

1. `log()` is called concurrently from many producer threads. It must **never call the
 global allocator**, no `malloc`, no `new`, nothing that could contend on the heap's
 own lock or have unpredictable latency. No running to the supply closet mid-rush.
2. No two concurrent `log()` calls may ever write to the same record slot. A message
 from one producer must never be lost, overwritten, or interleaved with another
 producer's bytes, two people never end up sharing one line.
3. `drain()` is called from exactly one consumer thread. It returns every record that's
 been fully written so far, and must never return a record that's only partially
 written.
4. **This is a fixed-capacity buffer, not a wraparound ring**, once `capacity` records
 have been claimed, `log()` returns `false` and the caller decides what to do (drop the
 message, block, escalate). It does not silently overwrite old records, the sheet is
 full, not "erase the oldest signature and reuse the line."

## Why the constraints exist

**`message` is a fixed-size field**, truncate if the input is longer, don't allocate to
fit it. The line on the sign-in sheet is only so long; you don't get to tape on extra
paper.
