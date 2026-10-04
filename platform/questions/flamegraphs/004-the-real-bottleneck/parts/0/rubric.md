A good answer covers:

- **The arithmetic.** `log`'s total is 5+40+35 = 80 samples, double the single widest box
  in the graph (`db_query` at 40). The point isn't the specific numbers, it's noticing
  that summing scattered occurrences of the same name can beat the single largest visible
  box by a wide margin.
- **What the colleague missed and why.** They optimized based on what one glance can see
  — the single widest box — and a flame graph doesn't visually group same-named frames
  together by default (they can be anywhere, under any parent). Nothing about the shape
  of the graph makes 80-spread-across-three-boxes look more urgent than 40-in-one-box; if
  anything, the one big box draws the eye more. A good answer says explicitly that visual
  salience and actual cost are two different things here.
- **Why the search/highlight feature exists.** Because this exact blind spot is common
  enough that tool authors built a specific feature for it: searching by name and summing
  matches is the only reliable way to find a cost center that's spread thin across many
  call paths, since eyeballing widths can't do that aggregation.

NEEDS_WORK if the answer doesn't compute the correct total for `log`, or concludes
`db_query` is still the bigger cost center after being asked to add up the scattered
occurrences.

## What produced this data

```cpp
void handle_request(Request& r) {
    auth(r);           log("auth");           // main;handle_request;auth;log        -- 5
    db_query(r);        log("db_query");        // main;handle_request;db_query;log    -- 40
    render(r);           log("render");           // main;handle_request;render;log      -- 35
}
```
Nothing here is a bug — `log` is called correctly from three places. The mistake to
correct is diagnostic, not code: eyeballing the single widest box (`db_query`, 40) and
missing that `log`, summed across all three call sites, is actually the bigger cost
(80) — found by searching for the name, not by widening any one box.

