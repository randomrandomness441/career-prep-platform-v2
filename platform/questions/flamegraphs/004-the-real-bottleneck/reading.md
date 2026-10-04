## 1. Reframe

Width in one box tells you that call path's cost. It says nothing about a function's
*total* cost across every path that calls it — that requires summing across the whole
graph, which a glance can't do.

## 3. The broken version, first

The natural first instinct is "find the widest box, optimize it, repeat." That works
when one path really does dominate. It fails silently when the real cost is a shared
utility — a logger, a serializer, an allocator — called a little bit from everywhere.
Each call site looks innocent on its own. Nobody ever sees one box worth optimizing,
so nobody optimizes it, even though it's the single biggest line item once you add up
every appearance.

## 4. Interview follow-ups

- How would you actually find this kind of spread-out cost in a real, large flame graph
  with hundreds of frames, without manually eyeballing every box? Use the search feature
  built into flame graph viewers (function-name search with a total-percentage readout),
  or better, go one level below the graph and directly aggregate the folded-stack data by
  frame name with a script before ever rendering a picture. This is exactly what a real
  incident report from [OpenSearch's Java client](https://opensearch.org/blog/opensource-perf/)
  describes doing: searching a flame graph by name to isolate one component's activity
  from everything else running alongside it, which is what let them notice that a JSON
  parsing helper's own frame looked modest while what it was triggering underneath it
  wasn't — see question 009 for that shape of bug built as a real, run-it-yourself example.
- Does this same "scattered small cost" problem apply to off-CPU flame graphs too? Yes —
  the aggregation blind spot is a property of how flame graphs merge same-named frames
  under different parents, independent of whether the underlying samples are CPU time or
  off-CPU wait time.
