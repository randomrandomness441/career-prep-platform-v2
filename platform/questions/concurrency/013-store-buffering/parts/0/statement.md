# Sequential Consistency vs Relaxed: The Store-Buffer Litmus Test

## ELI5: two people posting notes on each other's doors

Alice and Bob live across the hall from each other. At the same moment, each of them
pins a note on their *own* door saying "I'm home," then walks over and checks the
*other* person's door to see if they're home too.

Common sense says no matter who finishes first, at least one of them should see the
other's note. If Alice checks Bob's door *after* Bob has already pinned his, she sees it.
If Bob checks first, before Alice has pinned hers, he sees nothing. But then Alice,
checking a moment later, must see Bob's note, because Bob definitely pinned his before he
went to check hers. Either way, somebody sees something. Both people finding an empty
door seems impossible.

Except: what if pinning a note isn't instant? What if each person actually scribbles the
note and holds it in their pocket for a fraction of a second before walking over to
actually stick it on the door, and during that fraction of a second, they've *already*
gone to check the other person's door? Then it's entirely possible for Alice to check
Bob's door while Bob's note is still in his pocket, and for Bob to check Alice's door
while *her* note is still in her pocket. Both come back saying "nobody's home," even
though both of them technically already wrote their note.

That pocket delay is real, inside a CPU. It's called a **store buffer**. A core doesn't
publish a write to memory the instant you make it. The write can sit in a small private
buffer for a short while before other cores can see it. `arriveA` and `arriveB` below are
exactly Alice and Bob: each announces its own arrival, then checks whether the other side
has already arrived.

## What you're actually building

```cpp
class Handshake {
public:
    int arriveA();   // returns 1 if it observed that B had already arrived, else 0
    int arriveB();   // returns 1 if it observed that A had already arrived, else 0
};
```

`arriveA()` is called exactly once, by one thread. `arriveB()` is called exactly once, by
a different thread, concurrently.

## The invariant

Reason about it as ordinary sequential events, ignoring hardware for a moment. Whichever
of the two calls *finishes second*, in real wall-clock time, must have started after the
other one's announcement was already made, so it should see it. It is not possible for
**both** calls to return `0`. One of the two writes happened first. By the time the
*other* thread's read happens, that write already occurred. There must always be at least
one `1` among the two results: someone has to see someone home.

## Requirements

1. Implement `Handshake` so that the invariant actually holds, every time, under real
   concurrent calls from two threads, not just "usually."
2. **No mutex, no condition variable, no blocking of any kind.** Both calls must return
   quickly regardless of what the other thread is doing. This models a genuinely
   lock-free fast-path check, not a rendezvous that waits for both sides. Alice and Bob
   each check once and move on. Neither stands at the door waiting.
3. State is exactly two flags, one per side. No third piece of shared state.

The boilerplate implements this with what looks like the obvious approach, and it
violates the invariant on every run. Run the tests before changing anything. The failure
is small (single digits per thousand trials) and easy to miss if you're not looking for
it, which is exactly what makes it dangerous in real code.
