## 1. Reframe

A flame graph is not a timeline. It's a merged histogram of "what was on the call stack
when a sample was taken," stacked by call depth. Every number and every position in it
comes from counting, not from clock time.

## 2. The broken version, first

Someone new to profiling opens a flame graph, sees `parse_json` sitting to the left of
`write_response`, and concludes parsing happens before writing the response. Sometimes
that's even true, by coincidence. But it's true for the wrong reason: the tool sorted
those boxes alphabetically ("p" before "w"), not by when they ran. Flip the function
names and the graph looks identical in shape but now implies the opposite order. If
your read of a flame graph depends on left-right position meaning "before/after," your
conclusion is not supported by anything the graph actually encodes, and you will
eventually chase a bug that isn't there.

The fix: only two things carry real information. Width (how many samples included that
frame — a proxy for how much CPU time or how often that code path ran) and vertical
position (who called whom). Left-right position among siblings is arrangement, not data.

## 3. Where this breaks

Width tells you *frequency of appearance in samples*, not wall-clock duration directly —
those are close to the same thing under CPU sampling, but they're not identical.
A function that's wide because it truly runs a lot looks the same as a function that's
wide because a lock made every caller sit there waiting on the same line of code (in
a CPU profile, a spinlock burns CPU while waiting, so it shows up wide, and you might
misdiagnose contention as "this function is slow" instead of "everyone is stuck waiting
for this function"). Width alone doesn't distinguish "does a lot of real work" from
"burns CPU while blocked" — that distinction needs more than the flame graph in front
of you (see 005, off-CPU flame graphs, for the other half of this picture).

## 4. Interview follow-ups

- If two flame graphs use different colors for the same function, does that mean
  something changed? No — color is usually just a hash of the function name or a random
  warm-color palette (unless the tool documents it otherwise, e.g. differential graphs
  use color deliberately, see 007). Never read meaning into color without checking what
  the specific tool's legend says it means. A real example where color *is* deliberate:
  Netflix's mixed-mode Java flame graphs use green for Java frames, yellow for C++, red
  for kernel/system code, with color *intensity* randomized only to help tell adjacent
  frames apart within the same hue — one picture showing every layer of the stack at
  once, which no single profiler could do before this convention existed.
- A junior engineer says "I made the widest box narrower, so I fixed the bottleneck."
  What's missing from that claim? They fixed *a* wide box. Whether it was *the* bottleneck
  depends on whether total end-to-end time actually dropped — width shrinking locally
  doesn't guarantee that on its own (see 004 for the "one function scattered across many
  small boxes" case where no single box was ever the real cost center).
