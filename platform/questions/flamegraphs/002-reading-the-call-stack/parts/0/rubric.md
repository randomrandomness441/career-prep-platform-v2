A good answer covers:

- **The root is `main` on both lines**, and its position (bottom of the stack, y=0) means
  it's the entry point — nobody called it, it's where execution started. A good answer
  distinguishes this from width: `main` being at the bottom is a *depth* fact, unrelated
  to how wide its box is.
- **The on-CPU frame for line 1 is `malloc`**, the topmost frame, not `parse_json`. This
  is the core test of the question — if the answer picks anything other than the topmost
  function, it has the y-axis backwards. `parse_json` and `json_alloc` were on the stack
  (waiting for the frame above them to return) but not themselves executing.
- **The stacking mistake:** at a glance, someone might read the merged block of
  `json_alloc` + `malloc` as "8 samples of cost, split evenly" or misattribute it to
  whichever label is easier to see, when really all 8 samples' actual CPU time belongs
  to `malloc` specifically (the frame with nothing above it). A good answer names this
  directly: you have to read the topmost label of a block, not just note that "something
  around here is 8 samples wide."

NEEDS_WORK if the answer picks a non-topmost function as the on-CPU frame, or treats
"bottom of stack" and "widest" as the same kind of fact.

## What produced this data

```cpp
void handle_request(Request& r) {
    Value v = parse_json(r);   // calls into json_alloc, which calls malloc
}
Value parse_json(Request& r) { return json_alloc(r.body); }
Value json_alloc(const std::string& body) { return Value(malloc(body.size())); }
```
When a sample lands during this call, `malloc` is the topmost, actually-executing
frame — `parse_json` and `json_alloc` are just waiting on it to return. The mistake to
correct is crediting the cost to whichever name is easiest to notice, not to the
frame that's actually on top.

