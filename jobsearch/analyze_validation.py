#!/usr/bin/env python3
"""Three-way diff against validate_results.jsonl (written by
validate_llm_vs_regex.py): regex alone, GLM-4.6 given the regex score as
context ("with_regex"), and GLM-4.6 with no regex score in the prompt at
all ("alone"). Answers two separate questions:

  1. Is the regex pre-filter (score >= 50) actually reliable -- compare it
     against the "alone" judgment, which was never anchored on it.
  2. Does showing the LLM the regex score bias its own answer toward it --
     compare "with_regex" against "alone" on the SAME postings.

    .venv/bin/python3 analyze_validation.py
"""
import json
import os
import statistics

# PREFILTER: the regex score gate this analysis checks against -- no longer
# a live filter in platform/server.py (dropped after this validation showed
# it wasn't reliable), kept here purely as the reporting threshold for "how
# many real matches would a score<50 cutoff have thrown away."
PREFILTER = 50
STRONG = 70

PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "validate_results.jsonl")


def corr(xs, ys):
    try:
        return statistics.correlation(xs, ys)
    except Exception:
        return float("nan")


def main():
    by_id = {}   # last row per id wins -- a retried failure appends a new line
    with open(PATH) as f:
        for line in f:
            r = json.loads(line)
            by_id[r["id"]] = r
    all_rows = list(by_id.values())
    rows = [r for r in all_rows
            if r.get("llm_with_regex_score") is not None and r.get("llm_alone_score") is not None]
    errored = len(all_rows) - len(rows)

    print(f"{len(rows)} complete pairs, {errored} rows with at least one error\n")
    if not rows:
        return

    regex = [r["regex_score"] for r in rows]
    with_r = [r["llm_with_regex_score"] for r in rows]
    alone = [r["llm_alone_score"] for r in rows]

    print("=== anchoring check: does showing the LLM the regex score bias it? ===")
    anchor_diff = [w - a for w, a in zip(with_r, alone)]
    print(f"mean(with_regex - alone):   {statistics.mean(anchor_diff):+.1f}")
    print(f"stdev of that diff:         {statistics.pstdev(anchor_diff):.1f}")
    print(f"correlation(with_regex, alone): {corr(with_r, alone):.2f}")
    print(f"correlation(with_regex, regex_score): {corr(with_r, regex):.2f}  "
          f"(higher than the line above = anchoring)")
    print()

    print("=== regex vs the independent (alone) LLM judgment ===")
    diffs = [a - g for a, g in zip(alone, regex)]
    print(f"mean diff (alone - regex): {statistics.mean(diffs):+.1f}")
    print(f"median diff:               {statistics.median(diffs):+.1f}")
    print(f"stdev of diff:             {statistics.pstdev(diffs):.1f}")
    print(f"correlation(regex, alone): {corr(regex, alone):.2f}")
    print()

    # The real question: postings the pre-filter would EXCLUDE (regex<50)
    # that the independent LLM judgment rates as a strong match.
    missed = sorted([r for r in rows if r["regex_score"] < PREFILTER and r["llm_alone_score"] >= STRONG],
                     key=lambda r: -r["llm_alone_score"])
    print(f"--- pre-filter FALSE NEGATIVES: regex<{PREFILTER} but alone-LLM>={STRONG} ({len(missed)}) ---")
    for r in missed[:25]:
        print(f"  regex={r['regex_score']:>3} alone={r['llm_alone_score']:>3}  "
              f"{r['company']:16} {r['title'][:42]:42}  {r['llm_alone_reason']}")

    overrated = sorted([r for r in rows if r["regex_score"] >= STRONG and r["llm_alone_score"] < 40],
                        key=lambda r: r["llm_alone_score"])
    print(f"\n--- regex FALSE POSITIVES: regex>={STRONG} but alone-LLM<40 ({len(overrated)}) ---")
    for r in overrated[:25]:
        print(f"  regex={r['regex_score']:>3} alone={r['llm_alone_score']:>3}  "
              f"{r['company']:16} {r['title'][:42]:42}  {r['llm_alone_reason']}")

    biggest = sorted(rows, key=lambda r: -abs(r["llm_alone_score"] - r["regex_score"]))[:15]
    print(f"\n--- biggest regex-vs-alone disagreements overall ---")
    for r in biggest:
        print(f"  regex={r['regex_score']:>3} alone={r['llm_alone_score']:>3} "
              f"(Δ{r['llm_alone_score']-r['regex_score']:+.0f})  {r['company']:16} "
              f"{r['title'][:42]:42}  {r['llm_alone_reason']}")

    below = [r for r in rows if r["regex_score"] < PREFILTER]
    print(f"\n--- pre-filter miss rate ---")
    print(f"of {len(below)} postings the pre-filter would exclude, "
          f"{len(missed)} ({100*len(missed)/max(1,len(below)):.1f}%) the independent LLM rated >={STRONG}")


if __name__ == "__main__":
    main()
