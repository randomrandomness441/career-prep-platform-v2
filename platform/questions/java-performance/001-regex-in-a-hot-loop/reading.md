## 1. Reframe

`Pattern.compile` parses a regex string into a state machine. That parsing is real work,
proportional to the regex's complexity, not the input's size. Paying for it once is
normal. Paying for it on every string you check is a self-inflicted tax with no purpose.

## 3. The broken version, first

Measured on this machine, 200,000 strings against `"user-\\d+-active"`:
`s.matches(regex)` in a loop: 98.1ms. Same check, `Pattern` compiled once outside the
loop, `Matcher` created per string: 13.4ms. About 7x, and this ratio gets worse the more
complex the regex is, since a gnarlier pattern costs more to parse each time. The fix
never changes: compile once, reuse the compiled form.

## 4. Interview follow-ups

- Why doesn't the JIT just notice the regex string is a compile-time constant and
  hoist the compilation out of the loop automatically? `Pattern.compile` isn't a pure,
  side-effect-free operation from the JIT's point of view (it does real work and returns
  a heap object) — this class of optimization needs a human to do it explicitly, the JIT
  won't rewrite algorithmic-shaped code for you the way it might inline a small method.
- Does this same mistake show up outside regex? Yes — any factory-shaped call sitting
  behind an innocent-looking method name is the same trap: an ORM query building a new
  execution plan every call instead of caching a prepared statement, a JSON library
  re-deriving reflection metadata for the same class on every serialization, a logger
  reformatting the same pattern string per line. The common thread is "an object that's
  expensive to construct and safe to reuse, built fresh anyway."
