Plain text answer, three numbered points. Example shape:

```
1. perf script just dumps every sample as its own full stack trace -- no merging.
   flamegraph.pl needs one line per unique chain with a count already summed, so
   something has to deduplicate and count first. That's stackcollapse's whole job.
2. ~10ms between samples at 99Hz. A 2ms function has roughly a 1-in-5 chance of
   being caught by a nearby sample, so at this rate you'll often miss it entirely,
   and even a hit or two is too little signal to trust.
3. No, not directly -- 1,200/8,000 is 15%, 1,200/12,000 is 10%. Different cost
   share even though the raw count matches. Convert to a percentage of each run's
   own total before comparing.
```
