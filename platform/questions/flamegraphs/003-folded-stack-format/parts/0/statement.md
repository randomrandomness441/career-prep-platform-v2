## ELI5: from raw photos to the merged picture

The camera doesn't hand you a finished picture. It hands you a shoebox of raw photos —
thousands of them, each one just a list of names: "head chef, line cook, dishwasher."
Before you can draw the merged kitchen picture, someone has to sort that shoebox: group
every photo with the exact same list of names together, and write down how many photos
are in each group. Only then can you draw boxes sized by those counts.

That sorted, counted shoebox — one line per unique chain of names, with a count — is
what real profiling tools actually produce and consume. The pretty picture is the last
step, not the first.

## What you're actually building (understanding)

On Linux, the real pipeline for a CPU flame graph looks like this:

```
perf record -F 99 -a -g -- ./my_program     # take the raw "photos" (stack samples)
perf script                                  # dump them as readable stack traces
stackcollapse-perf.pl                        # sort the shoebox: one line per unique
                                              # chain, semicolon-joined, with a count
flamegraph.pl > out.svg                      # draw the boxes
```

The output of that third step, for every function-name profiler (Java's async-profiler,
Python's py-spy, Go's pprof after conversion), looks the same regardless of language:

```
main;handle_request;parse_json 12
main;handle_request;write_response 30
```

This is called the **folded stack format**. It's the actual interchange format almost
every flame graph tool reads.

## Requirements

1. Why does the pipeline need a separate "collapse" step at all — why can't
   `flamegraph.pl` just read `perf script`'s raw output directly?
2. `perf record -F 99` samples 99 times a second. If the real bottleneck is a function
   that runs for 2 milliseconds and then returns, roughly how likely is a single sample
   to land inside it, and what does that suggest about trusting a flame graph built at
   this sampling rate to catch it?
3. Two folded-stack files, captured from the same program on two different runs, might
   have different total sample counts (say 8,000 vs. 12,000). Can you compare a frame
   that's "1,200 samples wide" in the first file directly against "1,200 samples wide"
   in the second and conclude they cost the same? What would you need to do instead?

## Why this matters

If you only ever look at the finished picture, you'll assume it's more precise than it
is. Knowing it's built from a sampled, counted, then-drawn pipeline tells you exactly
where its blind spots come from — which is the entire subject of the next few questions.
