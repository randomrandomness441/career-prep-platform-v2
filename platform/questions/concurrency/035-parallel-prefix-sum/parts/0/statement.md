# Parallel Prefix Sum (Scan)

## ELI5: "how many people are standing in front of me?"

Picture a long line of people, and every single person wants to know: "how many people,
total, are standing in front of me?" The person at the very front already knows: zero.
Everyone else's answer depends on everyone before them, so this *looks* like something
you can only do one person at a time, front to back.

Here's the trick to doing it with a team instead: split the line into chunks, and hand
each chunk to a different helper. Each helper first counts how many people are in *their
own* chunk (that part's fully independent, no waiting needed). Then, one quick pass:
each chunk needs to know the total headcount of every chunk *before* it, and once it has
that one number, every person inside the chunk can compute their own answer instantly by
adding "people before my chunk" to "people before me within my chunk." Most of the work
happens fully in parallel; only that one small handoff between chunks has to happen in
order.

## What you're actually building

Compute the **exclusive** prefix sum of an array, using multiple threads:

```cpp
std::vector<long> parallel_prefix_sum(const std::vector<long>& input, int num_threads = 8);
```

`result[i]` must equal the sum of every element *before* index `i`, `result[0] == 0`,
`result[1] == input[0]`, `result[2] == input[0] + input[1]`, and so on. Exactly "how many
[units] are in front of position `i`."

## Requirements

1. Exact for any input, including negative numbers, zero, and an empty array.
2. Correct regardless of whether `input.size()` divides evenly by `num_threads`, chunks
 don't have to be perfectly even.
3. **Actually uses multiple threads to do the work**, a sequential loop that happens to
 produce the right numbers is not the point of this exercise.

## Why the constraints exist

**Element `i`'s answer depends on every element before it**, the algorithm has to
account for that dependency somehow; it can't be ignored just because the loop is split
across threads. That's exactly the "each chunk needs its predecessors' total first"
handoff from the story above.
