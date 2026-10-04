## ELI5: the old bridge sign

A bridge has had a sign for decades: "Trucks over 2 tons: DO NOT CROSS. Bridge may
collapse." The bridge got structurally reinforced years ago, but nobody replaced the
sign. Two different drivers get this wrong in opposite directions. One refuses to
cross at all, trusting the old sign completely, even though the bridge has been fixed
for years — overly cautious, based on outdated information. The other decides "signs
like this are usually outdated, I'll just drive across whatever's parked on it,"
without checking anything — reckless, based on no information at all. The only correct
move is to check the bridge's actual current inspection record for *this specific
bridge*, not to trust the old warning blindly or dismiss it blindly.

## What you're actually building (understanding)

"Catastrophic backtracking" (ReDoS) is real and extremely well documented: certain
regex patterns — classically `(a+)+`, `(a|aa)+`, `(a*)*` — force a backtracking engine
to try exponentially many ways of splitting up a run of matching characters before
giving up on a near-miss input, and this is repeated as gospel warning advice across
blog posts, security checklists, and interview prep material for `java.util.regex`
specifically.

Here's what actually happened when each of those three classic patterns was tested
against a near-miss input, on this machine, JDK 21, up to 50 repeated characters
(where a genuinely exponential blowup would already take an astronomically long time):

```
(a+)+     vs "aaaa...a!" (n up to 40):  every case, 0ms
(a|aa)+   vs "aaaa...a!" (n up to 50):  every case, 0ms
(a*)*     vs "aaaa...a!" (n up to 50):  every case, 0ms
```

None of them blow up. The reason: OpenJDK 9 added a real optimization to
`java.util.regex.Pattern` — when a greedy repetition fails to match starting at a given
input cursor position, that failure gets memoized. If backtracking tries the same
repetition at the same cursor position again, it's skipped instead of retried,
collapsing an enormous amount of the classic exponential retry pattern. This is
documented as *partial* mitigation, explicitly not a claim that every possible
catastrophic pattern is now safe.

## Requirements

1. A colleague, prepping for an interview, says: "nested quantifiers like `(a+)+` are
   always a ReDoS risk in Java, that's just a fact, never write one." What's wrong with
   this statement as a blanket claim, given what was actually measured above?
2. A different colleague, having read about this optimization, says: "OpenJDK fixed
   ReDoS, so I don't need to think about catastrophic backtracking in Java anymore."
   What's wrong with *this* statement, given how the fix is actually described?
3. The memoization specifically tracks "did this single repetition group already fail
   at this cursor position." Reasoning only from that description — not from a specific
   pattern you've tested — what characteristic of a regex construction seems likely to
   still be able to defeat this particular optimization? You don't need a verified
   example; reason about what the fix's own mechanism does and doesn't cover.

## Why this matters

Both wrong answers here sound like reasonable engineering positions, and both are
common in real teams. The actual skilled position isn't "yes" or "no" — it's knowing
there's a specific, checkable fact underneath the folklore, and being willing to check
it instead of reciting either the old warning or its dismissal.
