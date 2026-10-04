# ATS Scorer — Formal Spec (for review, not yet implemented)

Grounded in the 10 searches just run on real ATS/resume-matching research (sources at
bottom). Regex's job shrinks to title/seniority filtering only. This spec becomes the
rubric an LLM call executes — it is not meant to be hand-coded in regex.

## 1. What regex still does (and only this)

- Classify title: hand-on engineering role vs not (recruiter/sales/field-eng/etc.).
- Classify seniority tier from the title string (mid / senior / staff / manager).
- Location filter (India/Bangalore/Bengaluru/Remote-India).
- Date filter (posting age).

No skill matching, no keyword coverage, no scoring. That all moves to the LLM.

## 2. Inputs

- **Resume**: title/headline, summary, skills list (flat), work-experience bullets
  (each bullet tied to a role + date range).
- **JD**: raw posting text.

## 3. Per-requirement evidence model

The JD is reduced to a set of **requirements** `R = {r_1 ... r_n}` — hard skills, tools,
domain asks, years-of-experience bar. For each `r_i`, the candidate's evidence is a
tuple:

```
e_i = (presence, location, depth, recency)
```

- **presence** ∈ {0, 1} — does `r_i` appear in the resume, by exact term OR semantic
  equivalence (JD says "React", resume says "React.js" — same skill)? Research finding:
  vocabulary mismatch (not true absence) is the dominant documented cause of real ATS
  false-negative rejections. Regex cannot judge semantic equivalence; an LLM can.
- **location** ∈ {title, summary, bullet, skills-list-only} — placement is a real,
  separately-documented ATS weight: visible/early sections outweigh a buried mention.
- **depth** ∈ [0, 1] — does the mention carry a quantified outcome ("reduced p99
  latency 40% on a Kafka-based pipeline") vs a bare name-drop ("Kafka" in a skills
  list)? Research finding, direct quote: *"NLP infers skill proficiency from phrases
  like 'led a team of 12' or 'reduced costs by 30%'"* and *"evidence still beats
  declaration every time."* This is the exact distinction Sourav asked for — a skill
  proven in a bullet should outweigh the same skill only declared in a tag list.
- **recency** ∈ [0, 1] — a skill demonstrated recently and over a sustained duration
  outweighs the same skill from a short stint years ago. Research finding: *"a Python
  project from 8 years ago counts less than one from last year."*

### Per-requirement weight

```
w_i = presence_i × location_weight(location_i) × depth_i × recency_i
```

`location_weight`: title/summary > bullet > skills-list-only (skills-list-only is not
zero — a real Skills section is real resume content — just the weakest tier).

### Coverage (quality-weighted, not a raw count ratio)

```
Coverage = (1/n) × Σ w_i     for i = 1..n
```

### Anti-stuffing property

Repeating a keyword must not linearly inflate `w_i`. BM25's term-frequency saturation
is the established mathematical fix: more mentions help, with diminishing returns, not
linearly — closes the keyword-stuffing exploit by construction rather than by denylist.

### Experience-fit multiplier

```
X = f(years_candidate, years_required)   -- penalizes clear under-qualification,
                                             doesn't reward over-qualification further
```

### Final score

```
Score = 100 × clip(Coverage, 0, 1) × X
```

## 4. Why this has to be an LLM call, not regex

Every one of the four evidence dimensions needs real language understanding:
semantic equivalence, whether a nearby number actually supports the claimed skill (not
just "a digit exists on this line"), and connecting a date range to the specific skill
used in that role. A regex approximation of any of these is exactly the kind of fragile
heuristic that caused today's repeated bugs. An LLM given this rubric plus the full
resume and JD text can execute the formula directly and return structured JSON: score,
per-requirement breakdown (presence / location / depth / recency / weight), matched,
semantically-matched, and missing lists, with reasoning. This is also just how real
modern systems already work — confirmed directly: 48-83% of companies now use AI in
resume screening, typically a zero-shot LLM evaluator scoring "skill match" and
"experience relevance," returning JSON.

## 5. Open question for Sourav before anything gets built

The old design ran regex (free) on every posting and reserved the LLM for an explicit,
opt-in "AI-judge" pass — specifically to cap LLM spend. Under this spec, there is no
score at all without an LLM call. Two ways to reconcile that:

- **(a)** LLM-judge becomes the only scorer, still manually triggered per batch (not
  automatic on fetch) — same spend-control principle, just applied to every posting
  that needs a score at all, not a secondary pass.
- **(b)** Keep a trivial, honestly-labeled regex pre-filter (title/location/date only,
  already item 1 above) to cut the candidate set before any LLM call, so spend scales
  with "postings that survive title/location filtering," not every fetched posting.

(b) is really just making explicit what's already true structurally (title/location
filtering already happens first) — flagging it so the spend tradeoff is a conscious
choice, not a surprise.

## Sources (from the 10 searches)

- [ATS Resume Optimization: The Ultimate 2026 Guide](https://blog.theinterviewguys.com/ats-resume-optimization/)
- [How ATS Scores Your Resume Keywords](https://jobwizard.ai/blog/ats-resume-optimization-part-2-keyword-matching-deep-dive-and-scoring-algorithm-explained)
- [Skills Section Resume ATS: Where Placement Changes Your Score](https://www.profileops.com/en/blog/skills-section-resume-ats)
- [Resume Skills Section ATS Reads Correctly](https://hireflow.net/blog/resume-skills-section-ats-reads-correctly)
- [Quantifying Algorithmic Friction in Automated Resume Screening Systems (arXiv)](https://arxiv.org/pdf/2602.04087)
- [The Algorithmic Barrier (arXiv)](https://arxiv.org/pdf/2601.14534)
- [Semantic Candidate–Job Matching: Dense Embedding Models (arXiv)](https://arxiv.org/html/2609.23307)
- [conSultantBERT: Fine-tuned Siamese Sentence-BERT for Matching Jobs and Job Seekers (arXiv)](https://arxiv.org/pdf/2109.06501)
- [Term Frequency Normalisation Tuning for BM25 and DFR Models](http://ir.dcs.gla.ac.uk/smooth/he-ecir05.pdf)
- [What is BM25? (Comparison with TF-IDF)](https://medium.com/@jinmochong/what-is-bm25-comparison-with-tf-idf-and-beyond-5a740479214b)
- [Competence-Level Prediction and Resume & Job Description Matching (arXiv)](https://arxiv.org/pdf/2011.02998)
- [How to Quantify Resume Bullet Points](https://resume.io/blog/quantify-your-resume-bullets)
- [How Far Back Should a Resume Go? Recency rules](https://www.resumatic.ai/articles/how-far-back-should-my-resume-go)
- [Human and LLM-Based Resume Matching (ACL Findings 2025)](https://aclanthology.org/2025.findings-naacl.270.pdf)
- [Measuring Validity in LLM-based Resume Screening (arXiv)](https://arxiv.org/html/2602.18550v1)
