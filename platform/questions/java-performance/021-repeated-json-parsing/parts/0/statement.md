## ELI5: hiring a new translator for every single sentence

Imagine needing a document translated, sentence by sentence, and instead of hiring one
translator who reads the whole document, you hire and train a brand-new translator from
scratch for every single sentence — teaching them the language, the domain vocabulary,
and your formatting preferences, then throwing them away the moment they finish one
sentence. The actual translation work is small. The hiring-and-training overhead,
repeated every single time, dwarfs it.

This is exactly what happens when a Java service creates a new `ObjectMapper` (the
class most JSON libraries use to convert between JSON text and Java objects) for every
request instead of building one and reusing it.

## What you're actually building (understanding)

Real, measured on this machine — real Jackson 2.17.0, downloaded and run directly, not
quoted from someone else's blog post:

```
new ObjectMapper per call (20,000 calls): 665.6ms
shared ObjectMapper       (20,000 calls):  22.9ms
```

About **29x** slower when a fresh `ObjectMapper` is constructed on every call. This is
the same shape of bug as question 001's `Pattern.compile` in a loop — an object that's
expensive to build and completely safe to reuse, built fresh anyway — but the
construction cost here is bigger and does more: an `ObjectMapper` isn't just parsing
setup, it scans the target class using reflection, builds and caches a full
serializer/deserializer strategy for it, and initializes several internal caches. None
of that work needs repeating for the same class on a second call, which is exactly why
reuse is so much faster.

A real production incident report describes a related, worse version of this mistake:
a message-handling function that scanned the same JSON payload **twice** and copied it
**roughly four times**, per message, **per connected client** — meaning the real
multiplier wasn't just "redundant work," it was redundant work repeated once for every
client watching the same message.

## Requirements

1. `Pattern.compile` (question 001) and `ObjectMapper` construction are the same shape
   of mistake, but the measured gap here (29x) is roughly 4x bigger than question 001's
   regex gap (roughly 7x). What does `ObjectMapper` construction actually do that a
   regex compile doesn't, that would explain a bigger gap?
2. The real incident above scans and copies a JSON payload multiple times, once **per
   connected client** receiving the same message. If a 2MB payload update needs to reach
   10,000 connected clients, what's the real, concrete cost difference between "parse
   once, then fan out the parsed result to all 10,000 clients" and "re-parse and re-copy
   once for every client"?
3. `ObjectMapper`'s tree-model API (`JsonNode`) is easier to write against than its
   streaming API, but published measurements show tree-model parsing can use 2 to 14
   times the memory of the raw JSON text, versus the streaming API's roughly constant
   memory use regardless of document size. Given that gap, when would you actually
   accept the streaming API's added complexity in real code?

## Why this matters

This exact mistake shape — an expensive, reusable object rebuilt on every call — keeps
reappearing under a different name in almost every part of a real stack: a compiled
regex, a configured JSON mapper, a database connection, a compiled SQL prepared
statement. Recognizing the shape once means recognizing it everywhere.
