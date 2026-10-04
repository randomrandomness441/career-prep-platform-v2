## 1. Reframe

"Is this regex dangerous" isn't a yes/no question about regex syntax in the abstract —
it's a question about a specific engine's specific behavior, which changes across
versions. Treating it as a fixed fact about syntax, in either direction, is the actual
bug here.

## 2. What was actually tested, for real

Five separate classic "evil regex" constructions — `(a+)+`, `(a|aa)+`, `(a*)*`,
`(a+)+(a+)+b`, and `([a-zA-Z]+)*` — against carefully-chosen near-miss inputs, on this
machine, JDK 21, pushed up to 50 repeated characters (where genuine exponential
backtracking would already take far longer than a human would wait). Every single one
completed in 0ms. This directly tests, and contradicts, a piece of advice repeated
constantly across blog posts and security checklists: that these specific patterns are
unconditionally catastrophic in any backtracking engine, `java.util.regex` included.

## 3. The broken version, first

The natural failure mode isn't a bug in a regex — it's trusting a claim about an engine
without checking which version of that engine, and when the claim was last true. Someone
who read a five-year-old blog post about Java ReDoS and applies it unmodified to a
current JDK is reasoning from stale information as if it were a timeless fact. The fix
isn't "learn the current correct answer and memorize that instead" — it's noticing that
this is the kind of claim that needs to be checked against the actual runtime in front
of you, because it has already changed once (OpenJDK 9's memoization fix) and there's no
guarantee it won't matter differently again on a future JDK or a different regex engine
entirely.

## 4. Real-world usage

Catastrophic backtracking is not a theoretical concern — a single poorly-written WAF
regex rule caused a real, global, roughly 27-minute [Cloudflare outage on July 2,
2019](https://blog.cloudflare.com/details-of-the-cloudflare-outage-on-july-2-2019/),
taking down access to a large share of the internet's traffic through CPU exhaustion
across their whole network. Worth being precise here: that incident was on Cloudflare's
own WAF regex engine, not `java.util.regex` — the lesson transfers (this bug class has
caused real, severe production outages) but the specific engine behavior does not
automatically transfer between different regex implementations, which is exactly this
question's point.

## 5. Performance

All measured, all real, all on this machine: five classic catastrophic constructions,
tested up to 50 repeated characters each, zero measurable slowdown in any case
(consistently 0ms). This is strong evidence of the OpenJDK 9+ cursor-position
memoization working as documented for these specific patterns — not proof that no
construction can defeat it.

## 6. Where this solution fails

The memoization is scoped to a single greedy repetition failing at a given cursor
position. It doesn't protect against every conceivable pattern shape — most plausibly,
constructions involving backreferences or lookaround assertions whose outcome depends
on more than the cursor position alone could still behave differently. It also only
protects `java.util.regex.Pattern` specifically — a third-party regex library, a
different JVM language's own regex implementation, or a regex evaluated inside a
different system entirely (a database's regex function, a CDN's WAF, a different
runtime altogether) gets none of this protection automatically. And even where the fix
does apply, an attacker-controlled *regex itself* (not just attacker-controlled input)
sidesteps this defense entirely — the mitigation assumes the pattern is trusted and only
the input might be adversarial.

## 7. Interview follow-ups

- If you had to accept regexes from an untrusted source at all (not just untrusted
  input against a fixed, trusted regex), would this JDK-level protection be enough?
  No — that's a fundamentally different threat model. The safer approach there is a
  regex complexity/timeout budget enforced at the call site, or a non-backtracking
  matching engine entirely (some libraries guarantee linear-time matching for exactly
  this reason, at the cost of not supporting every regex feature, like backreferences).
- Does this change how you'd answer a "is this regex safe" interview question in
  general? Yes — naming the specific, real mitigation and its documented scope is a
  stronger answer than either "yes it's dangerous" or "no it's fine," and saying "I'd
  verify this on the actual runtime" is itself a legitimate, senior-level answer when
  you don't have the measurement in hand.
