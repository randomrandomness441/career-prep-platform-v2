## ELI5: who's actually holding the pan

Back in the kitchen. A photo catches the head chef, who called the line cook, who
called the dishwasher, who is right now scrubbing a pan. Four people could be named
for this one photo, but only one of them has their hands busy at the exact instant
the camera clicked: the dishwasher, at the top of that chain. The head chef and line
cook aren't idle — they're each waiting on the person they called — but the camera's
"who is actually doing something right now" answer is always whoever is on the very
top of that stack of names.

Stack a thousand such photos by "who called whom," and the person at the bottom of
every stack (whoever nobody called — they just started working) is the root. Whoever
is at the top of each individual stack of names is the one who was actually, physically,
doing the work when that photo was taken.

## What you're actually building (understanding)

Same folded-stack format as before — each line is one full call chain, bottom to top,
with a sample count:

```
main;handle_request;parse_json;json_alloc 8
main;handle_request;parse_json;json_alloc;malloc 8
main;handle_request;write_response;flush_socket;write 30
main;handle_request;write_response;flush_socket;write;syscall_write 30
```

## Requirements

1. What sits at the very bottom of both stacks here, and what does its position (not
   its width) tell you about it?
2. For the first line, which single function was actually executing — on-CPU, doing
   real work — at the moment those 8 samples were taken? Is it `handle_request`,
   `parse_json`, `json_alloc`, or `malloc`?
3. In a real flame graph built from this data, `json_alloc` and `malloc` would be drawn
   directly on top of each other, both 8 samples wide, for those 8 samples. If you only
   glance at the graph's overall shape without reading each frame's label, what mistake
   might you make about where those 8 samples of "cost" actually belong?

## Why this matters

Every optimization decision starts with "what was actually running." If you optimize
`parse_json` when the real cost is in `malloc` three frames above it, you'll change code
that was never the bottleneck. The top frame is the only one that was ever truly
executing; everything below it was just waiting for the frame above to return.
