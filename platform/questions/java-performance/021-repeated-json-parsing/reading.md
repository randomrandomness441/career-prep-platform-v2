## 1. Reframe

An `ObjectMapper` (or any JSON library's equivalent configured entry point) is a
reusable, expensive-to-build object, not a lightweight function. Treating it like a
disposable value — one per call — is the same category of mistake as question 001's
regex, with a bigger constant cost because there's more real setup work behind it.

## 2. What was actually measured, for real

Real Jackson 2.17.0, downloaded directly from Maven Central and run on this machine:
constructing a fresh `ObjectMapper` per call and parsing a small JSON object 20,000
times took 665.6ms. Reusing one shared `ObjectMapper` for the same 20,000 parses took
22.9ms — about 29x faster. This independently reproduces (with a different exact
number, as expected — different JVM, different JSON shape, different measurement setup)
the same qualitative finding reported elsewhere: real-world fixes for this exact mistake
have reported 15x-40x improvements from switching to a shared, reused mapper.

## 3. The broken version, first

Creating an `ObjectMapper` inside a request handler, a utility method, or a helper
class's constructor is an extremely easy mistake to make, because it looks completely
local and harmless — "just set up what I need, right here, right before I use it." It
reads as good encapsulation. The cost only shows up under real request volume, and by
then it's spread across a codebase that treated `new ObjectMapper()` as a cheap,
disposable statement everywhere it was needed.

## 4. Real-world usage

A [real production incident report](https://github.com/ideaconnect/nuts/issues/122)
describes a message-handling function that ran a JSON-compacting scan into a fresh
buffer for every client, for every message — the first of two full scans and one of
roughly four copies performed per message per client. JSON parsing overhead has also
been reported sitting directly alongside Kafka writes as a real bottleneck in
high-throughput event pipelines, and Jackson's own ecosystem responded to the specific
cost of reflection-based field access with dedicated modules: **Afterburner**, and its
successor **Blackbird**, both generate real bytecode to replace reflection calls for
POJO field/method access, reporting roughly 20-40% additional speedup on top of an
already-reused `ObjectMapper` for read/write-heavy workloads.

## 5. Performance

All measured, all real, all on this machine, with the actual Jackson library (not a
simulation): 665.6ms (new mapper per call) vs. 22.9ms (shared mapper), 20,000 calls,
~29x. Independently reproducing a real-world reported range of 15x-40x from a different
source.

## 6. Where this solution fails

Reusing one `ObjectMapper` is safe for reading configuration and using it concurrently
across threads once fully configured — Jackson documents `ObjectMapper` as thread-safe
after configuration. It is *not* safe to keep mutating a shared mapper's configuration
(adding modules, changing features) while other threads are actively using it for
parsing — that's a real race condition, not a performance issue, and the fix is
configuring the mapper fully once at startup, before it's shared, never modifying it
afterward.

## 7. Interview follow-ups

- Does building a `Pattern` (question 001) and building an `ObjectMapper` for the *same
  class* twice cost the same the second time? For `ObjectMapper` specifically, no — it
  caches serialization/deserialization strategy per class internally, so *reusing one
  mapper instance* across many calls for the same class benefits from its own internal
  cache, on top of avoiding reconstruction entirely; a `Pattern` has no equivalent
  per-input cache, its entire benefit comes purely from not re-parsing the regex string.
- If a team can't easily refactor to a single shared `ObjectMapper` (many independent
  modules, no shared configuration point), is there a middle-ground fix? A pooled or
  cached-by-configuration approach — keying a small cache of `ObjectMapper` instances by
  their configuration, so equivalent configurations share an instance — trades a bit of
  architectural complexity for most of the reuse benefit without requiring one global
  singleton everywhere.
