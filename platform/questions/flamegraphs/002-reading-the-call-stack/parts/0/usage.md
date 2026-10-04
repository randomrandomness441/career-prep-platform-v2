No code here either. Answer the three numbered requirements as plain text. Example shape:

```
1. main is at the bottom of both stacks. That's a depth fact -- it's the entry
   point because nothing called it, not because of how wide its box is.
2. malloc. It's the topmost frame in that line, so it's the one actually
   executing when the sample was taken. Everything below it was just waiting
   for it to return.
3. You might think the cost is split between json_alloc and malloc, or credit
   it to whichever label you notice first. It isn't split -- all 8 samples'
   worth of on-CPU time belongs to malloc, the top frame. json_alloc never
   executed anything itself in these samples, it just called malloc and waited.
```
