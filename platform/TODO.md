# Platform engineering backlog

Not the course save-file (that's `../PROGRESS.md`) — this is engineering work on the prep
platform itself.

## Done: progress tracker across skill packs (2026-09-25)

Shipped: `db.pack_rollup()` groups `all_status()`'s output by `q["topic"]`, `/api/questions`
returns it as `packs: {topic: {solved, total, helped, racy, name}}`, and the list view shows
a chip per pack (solved/total + a bar) once more than one topic folder exists — hidden
entirely for the single-pack case, so this adds nothing to look at until a second pack shows
up. Clicking a chip filters the list to that pack; `#filter-select` also gets a "Pack: <name>"
option per topic, built dynamically instead of the old hardcoded
`pure-storage`/`coding`/`qa`-only list. Verified live in-browser: rollup renders with correct
counts, chip click filters to exactly that pack's questions, dropdown stays in sync.

Fixed in passing: the category badge in the list used to hardcode `qa → "Q&A"`, anything
else → `"Coding"` — wrong the moment a pack uses a category other than those two (caught via
the new `design` pack). Now renders the actual category string.

## Done: fork-ability doc (2026-09-25)

`platform/PACK-FORMAT.md` — the doc a stranger forking this repo actually needs: the
`compiled` vs `judged` question kinds, the folder layout for each, `pack.json` schema,
`meta.json` fields, what tracking/mock-interview/ask-a-teacher you get for free vs. what
still needs an LLM key or (for a non-C++ compiled pack) real `runner.py` work. Linked from
`AUTHORING.md`.

Depends on the pack decoupling work: `pack.json` per topic folder, the
`kind: "compiled" | "judged"` field on `meta.json`, and `load_pack()` in `server.py`
(2026-09-21). See `questions/design/001-rate-limiter/` for a working example of a
`judged`-kind question dropped into a brand-new pack with zero engine code changes.

## Open

- Nothing currently tracked. Next candidate, if it comes up: extending `runner.py` to
  compile a `kind: "compiled"` pack in a language other than C++ — real work, not a quick
  add, only worth doing if an actual pack needs it.
