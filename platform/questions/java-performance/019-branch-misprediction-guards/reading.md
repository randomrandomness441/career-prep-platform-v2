## 1. Reframe

"Avoid branches" is usually taught as a bit-twiddling trick for tight numeric loops.
The real, more broadly useful version of the lesson is architectural: a decision that's
checked constantly and unpredictably costs real, measurable time from misprediction
alone, independent of how much work the decision actually gates.

## 2. What was actually measured, for real

On this machine, JDK 21, a loop checking a boolean guard 50 million times, comparing
patterns with roughly the same total real work:

```
long, predictable runs of true/false:  184ms
truly random true/false, same split:   397ms
```

More than 2x slower for genuinely unpredictable input, doing the same amount of actual
work. This isolates the misprediction cost specifically, since both patterns take the
expensive path about half the time.

## 3. The broken version, first

The natural instinct is to think of a cheap boolean check as free — "it's just an `if`,
what could it cost." That's true when the branch is predictable. It stops being true the
moment the pattern of true/false becomes genuinely unpredictable, and nothing about
reading the code tells you which situation you're in — the same `if (enabled)` line
costs wildly different amounts depending purely on the runtime pattern of values flowing
through it, the same shape of "cost depends on data, not code" bug as question 011's
Integer cache boundary.

## 4. Interview follow-ups

- Sorting an array before processing it is a classic real-world trick that improves
  branch prediction for a filtering loop (`if (x > threshold)`). Why does sorting help,
  concretely? It turns a scattered, unpredictable sequence of true/false outcomes into
  long, contiguous runs of the same outcome — exactly the "predictable" pattern this
  question measured as fast, at the cost of the sort itself, which is only worth it if
  the loop runs many times over the same sorted data.
- Is branch misprediction cost the same on every CPU? No — the exact penalty (how many
  cycles a misprediction costs) depends on pipeline depth and the specific
  microarchitecture; deeper pipelines generally pay a larger misprediction penalty. The
  qualitative lesson (predictable beats unpredictable, for the same amount of real work)
  holds broadly; the exact multiplier is hardware-specific and worth measuring on your
  actual target, not assumed from one machine's numbers.
