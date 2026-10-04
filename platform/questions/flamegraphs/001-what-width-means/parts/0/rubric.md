A good answer covers:

- **Correct widest frame.** `background_gc` at 60 samples, wider than `handle_request`'s
  46 (12+4+30, summed across its children). The answer should show the addition, not just
  state the number — that's the part that proves they understand width accumulates up
  the stack from children to parent.
- **Horizontal position carries no time or order information.** Standard flame graph
  tools sort sibling frames alphabetically by default, purely so identical function names
  land next to each other and merge. `write_response` sitting to the right of
  `validate_input` says nothing about which ran first, second, or at all relative to each
  other in time. An answer that says "it looks like validate_input ran first because it's
  further left" is wrong and should be corrected directly, not softened.
- **The teammate is wrong, and the answer says so plainly.** The left-most function is
  whichever sorts first alphabetically among its siblings, not the entry point. `main`
  being at the bottom (not the left) is what makes it the entry point — that's a y-axis
  fact, not an x-axis one. A good answer keeps y-axis (depth/call relationship) and x-axis
  (frequency, alphabetically arranged) cleanly separate.

NEEDS_WORK if the answer treats x-axis position as chronological in any of the three
parts, or gets the widest-frame arithmetic wrong.

## What produced this data

Not a code bug to fix — this question is about reading a profile correctly. The source
that would generate the given folded-stack lines looks roughly like:
```cpp
void handle_request(Request& r) {
    parse_json(r);       // main;handle_request;parse_json   -- 12 samples
    validate_input(r);   // main;handle_request;validate_input -- 4 samples
    write_response(r);   // main;handle_request;write_response -- 30 samples
}
```
The mistake to correct isn't in this code — it's in reading `write_response` as having
run "after" `validate_input` just because it's drawn to its right in the rendered graph.

