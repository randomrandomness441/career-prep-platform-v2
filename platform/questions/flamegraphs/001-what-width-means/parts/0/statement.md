## ELI5: hidden cameras in a kitchen

Picture a restaurant kitchen. Every second, a hidden camera snaps a photo of everyone
working and who they're doing it for. The head chef is cooking a dish. To do it, they
call the line cook over. The line cook calls the dishwasher over to grab a clean pan.
One photo might catch all three at once, stacked: head chef, then line cook, then
dishwasher, in that order.

Do this for an hour. You now have thousands of photos. Instead of flipping through them
one at a time, you glue all of them together into one picture: everyone who was ever
"head chef → line cook → dishwasher" in the same photo gets merged into one stacked
block. The wider that block is, the more of the thousand photos caught that exact
chain happening. That merged picture is a flame graph.

## What you're actually building (understanding)

Here's real data from that kind of sampling: a "folded stack" file, one line per unique
call chain, with a count of how many photos caught it.

```
main;handle_request;parse_json 12
main;handle_request;validate_input 4
main;handle_request;write_response 30
main;handle_request;write_response;flush_socket 25
main;background_gc 60
```

If you rendered this as a flame graph: `main` is one wide base bar (100 total samples
sat above it). Above `main`, you'd see two blocks side by side: `handle_request` (46
samples wide: 12+4+30) and `background_gc` (60 samples wide). Which one is wider?
`background_gc`, even though it never called anything else.

## Requirements

Answer these, using the sample data above:

1. Which single frame is the widest box in the resulting flame graph, and how wide
   (in samples) is it?
2. `write_response` and `flush_socket` sit on top of each other in the graph. Does
   that tell you `write_response` called `flush_socket` before or after
   `validate_input` ran? Explain what the horizontal position of a box does and does
   not tell you.
3. A teammate looks at this data rendered as a flame graph and says "the left-most
   function is where the program starts." Are they right? Why or why not, given how
   the boxes at any one level get ordered?

## Why this matters

If you read the x-axis as a timeline, every conclusion you draw from a flame graph
is wrong from the start. It's the single most common mistake people make the first
time they open one. Get this right before anything else, because every other question
in this pack builds on it.
