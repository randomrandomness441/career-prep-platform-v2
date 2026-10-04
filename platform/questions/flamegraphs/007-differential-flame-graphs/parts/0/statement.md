## ELI5: before-and-after photos of the same kitchen

You move the toaster to a more convenient spot, sure it'll speed up breakfast service.
To prove it, you don't just take a new set of photos and squint at them next to last
week's — side by side, small differences in a busy kitchen are nearly impossible to
spot by eye, and you might miss that moving the toaster made the coffee station slower
because now someone has to walk around it.

Instead, you overlay the two photo sets and color each person red if they got *busier*
after the move, blue if they got *less busy*, sized by how much. One picture, colored by
change, instead of two pictures you have to compare in your head.

That's a **differential flame graph**: two folded-stack captures, same call structure,
colored by the difference between them instead of by anything else.

## What you're actually building (understanding)

Before and after folded-stack data for the same request handler, after a caching change:

```
BEFORE                                        AFTER
main;handle;db_query 60                       main;handle;db_query 15
main;handle;render 30                         main;handle;render 30
main;handle;send_response 10                  main;handle;send_response 10
                                               main;handle;cache_lookup 45
```

Total samples: 100 before, 100 after — same total, so a direct width comparison per
frame is fair here (real captures usually need normalizing to percentages first if
totals differ, per question 003).

## Requirements

1. Which frame got smaller, by how much, and which frame is brand new? Is the overall
   change here a clear win, a clear loss, or does it depend on something the numbers
   alone don't tell you?
2. `db_query` dropped from 60 to 15 — a real win. But `cache_lookup` appeared at 45,
   nearly as expensive as the win. What real-world situation would make this change
   still worth shipping, and what situation would make it a net loss you'd want to
   avoid claiming as a success?
3. A colleague says "the differential flame graph is green where things got better red
   where they got worse, so I can just look for the biggest red box and know that's
   what regressed." What's wrong with equating "red" directly with "the new bottleneck,"
   given what question 004 already established about scattered costs?

## Why this matters

"I made a change and the flame graph looks different" is not the same claim as "I made
it faster." Differential flame graphs exist specifically to make a change's actual net
effect checkable instead of assumed.
