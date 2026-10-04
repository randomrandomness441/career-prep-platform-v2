This is a `judged` question — there's no code to write. Type your answer as plain text
in the editor, numbered to match the three requirements, and hit Submit. Something like:

```
1. background_gc is widest at 60 samples. handle_request is 46 (12+4+30 summed
   across its children), so background_gc wins.
2. Nothing about before/after. Sibling frames are sorted alphabetically by the
   tool, not by time. write_response could have run first, last, or interleaved
   with validate_input -- the graph doesn't say.
3. Wrong. The left-most sibling is whichever name sorts first alphabetically.
   main is the entry point because it's at the bottom of the stack (y-axis),
   not because of its horizontal position.
```

A short, direct answer like this is enough -- the judge is grading whether the reasoning
is right, not word count.
