"""ATS keyword-coverage estimator.

Grounded in real, published ATS mechanics researched for this project, not
invented: keyword coverage is 40-50% of a typical ATS score, hard skills
(named technologies) outweigh soft skills, and placement matters -- a keyword
in the title or summary counts more than one buried in a bullet six lines
down. No vendor publishes their exact algorithm, so this is a labeled
ESTIMATE built on the mechanics that are actually documented, not a claim to
replicate any specific company's real scoring.
"""

import json
import re

STOPWORDS = {
    "the", "a", "an", "and", "or", "but", "of", "to", "in", "on", "for", "with",
    "at", "by", "from", "as", "is", "are", "was", "were", "be", "been", "being",
    "this", "that", "these", "those", "it", "its", "you", "your", "we", "our",
    "will", "would", "can", "could", "should", "may", "might", "must", "have",
    "has", "had", "do", "does", "did", "not", "no", "yes", "if", "than", "then",
    "so", "such", "into", "about", "across", "per", "etc", "including", "e.g",
}

# extract_keywords()'s "capitalized or tech-shaped token" heuristic for
# extra_jd_terms catches plenty of real tech terms (Azure, Anthropic, Apache)
# but also every sentence-initial word and every EEO-boilerplate/HR term a
# job posting carries, since those are capitalized too and aren't in
# STOPWORDS (which only holds lowercase function words). Found by checking
# the actual 15-most-common extra_jd_terms across 2000 real fetched postings
# -- "Ability", "Benefits", "Bachelor", "Company" et al. showed up hundreds
# of times each, never once a real skill. This is a denylist of the
# consistently non-technical ones, not an attempt at a general classifier.
NON_SKILL_CAPITALIZED_WORDS = {
    "ability", "abilities", "able", "act", "action", "actual", "additional",
    "additionally", "affirmative", "agent", "all", "angeles", "annual", "any",
    "applicant", "applicants", "application", "applications", "armed",
    "awareness", "acquisition", "ask", "account", "background", "bachelor",
    "base", "based", "belarus", "benefit", "benefits", "bonus", "bring",
    "build", "california", "candidate", "candidates", "care", "careers",
    "chance", "collaborate", "collaboration", "commitment", "company",
    "compensation", "compliance", "computer", "experience", "gender",
    "identity", "inclusion", "orientation", "race", "religion", "status",
    "veteran", "disability", "eeo", "equal", "opportunity", "employer",
}

# Soft-skill / non-technical words that show up constantly in JDs but aren't
# what a hard-skill ATS pass weighs heavily -- excluded from the "required
# hard skills" bucket even though they're real words worth having somewhere.
SOFT_SKILL_HINTS = {
    "communication", "collaborate", "collaboration", "leadership", "mentor",
    "mentorship", "ownership", "passion", "passionate", "team player",
    "fast-paced", "self-starter", "detail-oriented", "problem-solving",
}


# Previously a DENYLIST (title is "engineering" unless it contains one of
# ~30 known non-eng phrases) -- replaced after a real audit found 30+ junk
# titles slipping through it unnoticed across the live fetched data: Credit
# Risk Operations Associate, Accounts Payable Analyst, Chief of Staff,
# Executive Business Partner, Legal Entity Controller, Real Estate Analyst,
# Fraud Operations Team Lead, India Tax Lead, and "ML Annotation QA Engineer"
# (contains "Engineer" but is a data-labeling QA role, not software) among
# many others -- a denylist can only ever block phrasings someone already
# thought to list, and the real world has far more non-engineering job
# families than anyone will enumerate. Flipped to an ALLOWLIST instead: a
# title has to actually look like a real software/data/ML/infra engineering
# role (the actual target), not merely avoid looking like something else.
#
# Two rounds of this still leaked (QA Engineer - Endpoint; AI GTM Engineer,
# a marketing-ops role that only matched via a sloppy "AI within 20 chars of
# Engineer" proximity pattern; Associate Lead Data Scientist; Applied AI
# Architect, a customer-facing deployment role -- all confirmed by reading
# the real posting body, not title alone). The proximity patterns are gone,
# QA/Data-Scientist patterns are gone, and "architect" is now only allowed
# after software/backend/frontend/fullstack (not bare ai/ml/data + architect,
# which was pulling in consulting-flavored roles). "Applied AI" is denied
# outright: checked 4 real "Applied AI Engineer/Architect" postings across
# OpenAI/Anthropic and 3 of 4 were partner/customer-facing (only "GTM Growth
# Engineering" was genuine hands-on SWE) -- title text alone can't tell
# those apart (same prefix, different team mission in the JD body), so this
# defaults to excluding the whole family rather than risk more of the
# partner-facing majority.
#
# This is a structural limit, not something more regex tuning fixes: a
# title-only classifier cannot always know a role's real function when two
# teams at the same company use near-identical title phrasing for different
# jobs. Verified against all 6,252 distinct titles in the live fetched set --
# every title below was read, not sampled, for the final round; "IT Controls
# Data Engineer" / "IT Software Engineer, Infrastructure" / "Sr. IT Site
# Reliability Software Engineer" remain medium-confidence (plausible real
# infra roles, not individually read against their JD body) rather than
# fully verified.
_ENG_TITLE_ALLOW = [
    re.compile(r"\bsoftware\s+(development\s+)?engineer", re.I),
    re.compile(r"\bengineer,?\s*software\b", re.I),
    re.compile(r"\bsoftware\s+developer\b", re.I),
    re.compile(r"\bsoftware\s+architect\b", re.I),
    re.compile(r"\bsde[\s-]?(1|2|3|i|ii|iii)?\b", re.I),
    re.compile(r"\bsdet\b", re.I),
    re.compile(r"\b(backend|frontend|front-end|back-end|full[\s-]?stack)\s+(software\s+)?(engineer|developer|architect)\b", re.I),
    re.compile(r"\bengineer\b.*\b(backend|frontend|full[\s-]?stack)\b", re.I),
    re.compile(r"\b(data|ml|machine learning|ai|platform|infrastructure|infra|site reliability|sre|devops|dev ops|release|security|cloud)\s+(software\s+)?engineer\b", re.I),
    re.compile(r"\bmember\s+of\s+technical\s+staff\b", re.I),
    re.compile(r"\bmts\b", re.I),
    re.compile(r"\bmachine\s+learning\s+engineer\b", re.I),
    re.compile(r"\bapplication(s)?\s+engineer\b", re.I),
    re.compile(r"\bsite\s+reliability\s+engineer\b", re.I),
    re.compile(r"\bdevops\s+engineer\b", re.I),
    re.compile(r"\brelease\s+engineer\b", re.I),
    re.compile(r"\bsecurity\s+engineer\b", re.I),
    re.compile(r"\bsoftware\s+engineer\s+in\s+test\b", re.I),
    # language-named developer/engineer roles, independent of company -- a
    # real miss found in the audit ("Principal Java Developer - Distributed
    # Systems, Serverless" at Elasticsearch didn't contain the word
    # "software" at all).
    re.compile(r"\b(java|python|golang|go|c\+\+|c#|ruby|scala|kotlin|rust|node(\.js)?)\s+(developer|engineer)\b", re.I),
]

# Still needed even with an allowlist: a few title families would otherwise
# match an allow pattern above for the wrong reason -- "Forward Deployed
# [Software] Engineer" and "Solutions Engineer" both legitimately contain
# "engineer" next to real tech words, but are customer-facing/pre-sales
# roles, not the hands-on-building target; "ML Annotation QA Engineer"
# matched an earlier looser pattern but is data-labeling QA, not software;
# "Partner Applied AI Engineer"/"IT Service Engineer" are partner-facing and
# internal-corporate-IT roles respectively, confirmed by reading real
# postings (see the long comment above _ENG_TITLE_ALLOW).
NON_ENGINEERING_TITLE_WORDS = {
    "intern", "internship", "co-op",
    "solutions architect", "solution architect", "solutions engineer", "solution engineer",
    "sales engineer", "support engineer", "support developer",
    "forward deployed", "field engineer", "field engineering",
    "annotation", "qa engineer", "quality engineer", "data scientist",
    "applied ai",
    "partner engineer", "partner platform engineer",
    "it service engineer", "it systems engineer", "it software architect",
    "it support engineer",
}


def is_likely_engineering_title(title):
    low = title.lower()
    if any(w in low for w in NON_ENGINEERING_TITLE_WORDS):
        return False
    return any(p.search(title) for p in _ENG_TITLE_ALLOW)


def title_seniority_tier(title):
    """Rough seniority tier from the title alone. Complements (doesn't
    replace) years_required extracted from the JD body -- some postings
    never state years explicitly, but the title still names the tier
    plainly ("Staff", "Senior Staff", "Manager"). Order matters: check the
    more specific "senior staff" before the bare "staff" it contains."""
    low = title.lower()
    if re.search(r"\b(engineering\s+)?manager\b|\bdirector\b|head of engineering|\bvp\b|vice president", low):
        return "manager"
    if re.search(r"senior staff|sr\.?\s*staff", low):
        return "senior_staff"
    if re.search(r"\bstaff\b|\bprincipal\b", low):
        return "staff"
    if re.search(r"\bsenior\b|\bsr\.?\b|\bsde[\s-]?(iii|3)\b", low):
        return "senior"
    return "mid"


# Tighter than title_seniority_tier's "mid"/"senior" buckets (which caught
# "Senior Application Security Engineer" and "Senior Professional Services
# Engineer" -- genuinely senior-titled but not the actual target roles).
# This is the literal, specific title shape being targeted: SDE-2/II,
# Software Engineer 2/II, or Senior Software Engineer -- not "senior
# anything engineer."
_TARGET_TITLE_PATTERNS = [
    re.compile(r"\bsde[\s-]?(2|ii)\b", re.I),
    re.compile(r"\bsoftware\s+engineer\s*(2|ii)\b", re.I),
    re.compile(r"\bsenior\s+software\s+engineer\b", re.I),
    # flat-leveling companies (Cockroach Labs and similar) use this instead
    # of a numbered/seniority title -- the actual Cockroach Labs posting
    # this whole pipeline was first proven on wouldn't have passed this
    # filter without it. \bmts\b word-bounded so it can't match as a
    # substring of an unrelated word.
    re.compile(r"\bmember\s+of\s+technical\s+staff\b", re.I),
    re.compile(r"\bmts\b", re.I),
]


def matches_target_title(title):
    return any(p.search(title) for p in _TARGET_TITLE_PATTERNS)


# India/Bangalore/Bengaluru is the only target, not a toggle -- fetch_ats.py
# already enforces this at fetch time (a non-India posting is never saved),
# this is the same check applied again at read time. \b word-boundary match
# -- a plain substring check matched "Indiana" on "india", a real false
# positive found in the fetched data. Moved here (was in platform/server.py)
# so db.py can register it as a SQL function -- same reasoning as
# title_seniority_tier/is_likely_engineering_title living here: it's job-row
# classification logic, not web-server logic, and SQL-level filtering needs
# to call it per-row without a circular import back into the server module.
_INDIA_WORDS = ("india", "bangalore", "bengaluru")
_INDIA_RE = re.compile(r"\b(" + "|".join(_INDIA_WORDS) + r")\b")


def is_target_location(location, description_raw):
    loc = (location or "").lower()
    if loc:
        return bool(_INDIA_RE.search(loc))
    return bool(_INDIA_RE.search((description_raw or "")[:500].lower()))


def _tokenize(text):
    return re.findall(r"[A-Za-z][A-Za-z0-9+#./-]{1,}", text)


def _contains_skill(text_lower, skill_lower):
    """True if skill_lower appears in text_lower as a real token, not as a
    substring of a longer word. Plain `in` matching is wrong here: "RAG" is
    a substring of "storage" and "average", so a naive check flagged every
    JD that mentions storage as a RAG match -- caught by testing this
    against 937 real fetched postings and finding a recruiter role scoring
    100. Lookaround instead of \\b so symbol-bearing skills like "C++" and
    "CI/CD" still match correctly (\\b doesn't fire reliably around
    non-word characters)."""
    pattern = r"(?<![a-z0-9])" + re.escape(skill_lower) + r"(?![a-z0-9])"
    return re.search(pattern, text_lower) is not None


def extract_keywords(jd_text, known_skills):
    """Candidate hard-skill/requirement keywords from a job description.

    Two sources, combined: (1) any of the candidate's own known_skills
    (from db.skills_all()) that literally appear in the JD text -- these are
    the strongest signal, since they're real named technologies, not guesses.
    (2) capitalized or tech-shaped tokens in the JD that aren't in the known
    list -- these surface skills the JD wants that the candidate hasn't
    tagged yet, which is exactly the useful "you're missing this" signal.
    """
    jd_lower = jd_text.lower()
    matched_known = [s for s in known_skills if _contains_skill(jd_lower, s.lower())]

    tokens = _tokenize(jd_text)
    candidates = set()
    for tok in tokens:
        low = tok.lower()
        if low in STOPWORDS or low in NON_SKILL_CAPITALIZED_WORDS or len(tok) < 2:
            continue
        # tech-shaped: has a digit/symbol (C++, S3, k8s), or is capitalized
        # mid-sentence (a proper noun -- likely a product/tool name), or is
        # a known multi-char all-caps acronym (SQL, API, LSM).
        if re.search(r"[0-9+#./]", tok) or (tok[0].isupper() and tok.lower() not in STOPWORDS):
            candidates.add(tok)

    extra = sorted(c for c in candidates if c.lower() not in {s.lower() for s in matched_known})
    return matched_known, extra


# Patterns that name a minimum years-of-experience bar. Found by testing
# against real postings: a "Staff Software Engineer" role stating "10+ years
# of experience" was scoring 88 purely on keyword overlap, with the
# requirement never checked at all -- years-of-experience is a real, common
# hard filter in actual ATS/recruiter screening, not a nice-to-have.
_YEARS_PATTERNS = [
    # "8+ years of software engineering experience" -- up to a few words of
    # any qualifier between "years" and "experience", non-greedy so it stops
    # at the first "experience" rather than swallowing past it. A first cut
    # only allowed a short fixed adjective list here and missed this exact
    # case (JD said 8+, extractor returned 2 from a later, unrelated "at
    # least 2 years" mention) -- caught by testing against a real posting.
    re.compile(r"(\d{1,2})\+\s*years?(?:\s+(?:of\s+)?[\w/-]+){0,4}?\s*experience", re.I),
    re.compile(r"(\d{1,2})\s*-\s*\d{1,2}\s*years?\s*(?:of\s*)?experience", re.I),
    re.compile(r"(?:minimum(?:\s*of)?|at least)\s*(\d{1,2})\+?\s*years?", re.I),
    re.compile(r"(\d{1,2})\+?\s*years?\s*of\s*experience", re.I),
]


def extract_min_years(jd_text):
    """First years-of-experience requirement mentioned in the JD (document
    order -- usually the primary bar stated near the top of Requirements),
    or None if the JD never states one."""
    best = None
    for pat in _YEARS_PATTERNS:
        m = pat.search(jd_text)
        if m and (best is None or m.start() < best[0]):
            best = (m.start(), int(m.group(1)))
    return best[1] if best else None


def _experience_penalty(years_required, candidate_years):
    """Points to subtract for an experience-requirement gap. No requirement
    stated, or candidate meets/exceeds it (with half a year's slack for
    rounding): no penalty. Otherwise scales with the gap so a 2x mismatch
    (10+ required vs 4.5 actual) tanks the score instead of sitting at 88 --
    real recruiter/ATS screens do use years-of-experience as a hard filter."""
    if years_required is None or candidate_years is None:
        return 0
    gap = years_required - candidate_years
    if gap <= 0.5:
        return 0
    return min(60, round(gap * 12))


# The "required" set below is deliberately just the candidate's own known
# skills that show up in the JD -- that's the strong, unambiguous signal.
# But it makes the denominator self-referential: a JD matching only 2 of
# your skills reads as "100% coverage" of THOSE 2, even if the JD names ten
# other things you don't have at all. Caught this for real: "Senior
# Application Security Engineer" matched only Kubernetes+Python (its 80
# other real terms -- DAST, Cybersecurity, Bug Bounty -- never in your
# skill list, so invisible to "required") and still scored 75. Thin
# absolute evidence needs its own penalty, independent of the ratio.
MIN_STRONG_MATCHES = 4
EVIDENCE_PENALTY_PER_MISSING = 18


def _evidence_penalty(n_matched):
    if n_matched >= MIN_STRONG_MATCHES:
        return 0
    return (MIN_STRONG_MATCHES - n_matched) * EVIDENCE_PENALTY_PER_MISSING


# Evidence weighting, not a straight present/absent count. Grounded in real
# published ATS/resume-parsing research, checked directly before writing
# this (see session notes): "a skill named in the [skills] block and proved
# in an experience bullet is stronger than either one alone... evidence
# still beats declaration every time" -- and separately, that proficiency
# depth gets inferred "from phrases like 'led a team of 12' or 'reduced
# costs by 30%'", i.e. a quantified claim next to a skill is the regex-era
# proxy for real demonstrated depth, not a beginner name-drop.
#
# A skill present ONLY in the bare Skills-section line (never in a real
# point-bank bullet) still counts -- real ATS do scan a Skills section, it's
# not worth zero -- but at a fraction of a skill that's actually
# demonstrated in your own written work.
SKILL_LIST_ONLY_WEIGHT = 0.5
# A bullet mentioning the skill that ALSO has a number/percent/scale on the
# same line reads as a quantified, demonstrated claim rather than a mention
# in passing -- worth a real bonus over a bare bullet mention.
QUANTIFIED_BULLET_BONUS = 1.25


def _skill_evidence(bullet_lines, skills_line_lower, skill_lower):
    """Returns (found, weight) for one skill against the candidate's real
    content. Checks bullets first (strong evidence, bonus if quantified),
    falls back to the bare skills line (weak evidence)."""
    for line in bullet_lines:
        if _contains_skill(line.lower(), skill_lower):
            quantified = bool(re.search(r"\d", line))
            return True, (QUANTIFIED_BULLET_BONUS if quantified else 1.0)
    if _contains_skill(skills_line_lower, skill_lower):
        return True, SKILL_LIST_ONLY_WEIGHT
    return False, 0.0


def score_resume(resume_title, resume_summary, bullets_text, skills_line_text, jd_text,
                  known_skills, candidate_years=None):
    """Returns {score, matched, missing, breakdown} -- score is 0-100.

    bullets_text: the candidate's real point-bank prose (one point per line).
    skills_line_text: the bare comma-joined Skills-section text. Kept
    separate from bullets_text, not concatenated, specifically so a skill's
    evidence weight can depend on WHICH of the two it was found in."""
    matched_known, extra_candidates = extract_keywords(jd_text, known_skills)
    years_required = extract_min_years(jd_text)

    bullet_lines = [l for l in bullets_text.split("\n") if l.strip()]
    title_summary = f"{resume_title}\n{resume_summary}".lower()
    skills_line_lower = skills_line_text.lower()

    required = matched_known  # the real signal: known skills the JD actually asks for
    if not required:
        return {"score": 0, "raw_score": 0, "matched": [], "missing": [], "weak_evidence": [],
                "extra_jd_terms": extra_candidates, "years_required": years_required,
                "experience_penalty": 0, "evidence_penalty": 0,
                "breakdown": {"note": "No known skills from the point bank appear in this JD "
                                       "at all -- this posting may not be a real fit, or the "
                                       "point bank needs a skill this JD uses under a different "
                                       "name."}}

    matched, missing, weak_evidence = [], [], []
    placement_bonus = 0
    evidence_weight = 0.0
    for kw in required:
        low = kw.lower()
        found, weight = _skill_evidence(bullet_lines, skills_line_lower, low)
        if found:
            matched.append(kw)
            evidence_weight += weight
            if weight <= SKILL_LIST_ONLY_WEIGHT:   # tagged, but no bullet actually backs it up
                weak_evidence.append(kw)
            if _contains_skill(title_summary, low):
                placement_bonus += 1
        else:
            missing.append(kw)

    # Quality-weighted, not a plain count ratio -- a page of skill tags with
    # no bullet evidence behind them no longer reads the same as skills
    # you've actually demonstrated. Can run a little over 1.0 when several
    # skills are quantified-bullet-backed; raw_score's min(100, ...) is the
    # saturation cap on that, same spirit as BM25's term-frequency
    # saturation -- more/better evidence keeps helping, but not unboundedly.
    coverage = evidence_weight / len(required)
    placement_ratio = placement_bonus / max(len(matched), 1)

    # Published shape: keyword coverage ~40-50% of score, placement is a real
    # but secondary factor on top of raw coverage -- weighted, not asserted.
    raw_score = min(100, coverage * 75 + placement_ratio * 25)
    exp_penalty = _experience_penalty(years_required, candidate_years)
    evidence_penalty = _evidence_penalty(len(matched))
    score = round(max(0, raw_score - exp_penalty - evidence_penalty))

    return {
        "score": score,
        "raw_score": raw_score,
        "matched": matched,
        "missing": missing,
        "weak_evidence": weak_evidence,   # matched, but tag-only -- no bullet backs it up
        "extra_jd_terms": extra_candidates[:15],
        "years_required": years_required,
        "experience_penalty": exp_penalty,
        "evidence_penalty": evidence_penalty,
        "breakdown": {
            "known_skills_in_jd": len(required),
            "matched_in_resume": len(matched),
            "coverage_pct": round(min(coverage, 1.0) * 100, 1),
            "matched_in_title_or_summary": placement_bonus,
            "bullet_backed": len(matched) - len(weak_evidence),
        },
    }


def missing_skill_deltas(resume_title, resume_summary, bullets_text, skills_line_text, jd_text,
                          known_skills, candidate_years, live):
    """For every candidate term in `live["extra_jd_terms"]`, actually re-run
    score_resume as if that term were a real, tagged, evidenced skill -- added
    to BOTH the known_skills list (so it enters the scoring universe at all)
    and the resume body (so it counts as matched) -- and report the real
    score delta, not a guessed weight.

    live["missing"] is NOT what this should run on: it's structurally always
    empty, because the resume body passed into score_resume already has every
    one of the candidate's own tagged skills appended to it (skills_all()'s
    names), so any skill the JD wants that's also in known_skills is
    guaranteed to already "match". extra_jd_terms -- capitalized/tech-shaped
    JD tokens NOT in known_skills at all -- is the real "you haven't even
    tagged this" signal, and it's what was never actually surfaced in the UI.

    Diffs raw_score (coverage + placement only), not score. _evidence_penalty
    is a step function on len(matched): below MIN_STRONG_MATCHES it's a flat
    per-point-missing penalty, at or above it's zero. Diffing the final
    `score` meant crossing that threshold got credited in full to whichever
    single term happened to be the one that tipped the count over -- found by
    testing a real job with 3 matched skills (one below the threshold):
    every one of its 15 extra_jd_terms, real skill or not ("Comfortable",
    "HOW", "Client"), showed the identical +18, because literally any one of
    them pushed matched from 3 to 4 and zeroed the same penalty. That's a
    threshold artifact of the aggregate evidence count, not something any
    single term earns on its own -- diffing raw_score instead gives the
    coverage/placement credit a term actually, individually earns, with
    evidence_penalty and experience_penalty held fixed at their real values.

    These are looser than a curated skill (any capitalized or tech-shaped
    token), so the delta here is a real re-run of the exact scorer, not
    precision-guaranteed input -- some entries will be noise (company names,
    generic capitalized words) worth a human glance before trusting them.

    The one simulated placement that matters: the hypothetical term is added
    to resume_summary, not resume_body. coverage is already 100% for every
    job (known_skills gets appended into the body wholesale, so a required
    skill always trivially "matches" -- the same reason `missing` is always
    empty), so raw_score can only move via placement_ratio, the bonus for a
    matched skill showing up in title/summary specifically. A term buried in
    the body earns that bonus for nobody and always diffs to +0, true but
    useless. A real candidate who actually has a skill would put it in their
    skills/summary line, not bury it mid-paragraph -- simulating that
    realistic placement is what makes the number mean something."""
    deltas = {}
    for kw in live.get("extra_jd_terms", []):
        hypothetical_summary = resume_summary + "\n" + kw
        hypothetical_skills = known_skills + [kw]
        r = score_resume(resume_title, hypothetical_summary, bullets_text, skills_line_text, jd_text,
                          hypothetical_skills, candidate_years)
        deltas[kw] = round(r["raw_score"] - live["raw_score"])
    return deltas


# Real, specific resume-bullet rubric Sourav gave: "every bullet needs to be
# a keyword and/or a brag... if it doesn't have a keyword and/or a brag, it
# shouldn't exist." Keyword = a term from the JD's own qualifications/must-
# haves. Brag = a quantified claim, OR -- when there's no hard number -- an
# award, feedback, or named result. Checked against the real point bank
# before building this: 14 of 15 real bullets already pass (11 have a
# number, 3 more have a clear outcome clause with no digit --
# "cutting infrastructure cost", "keeping bounce/complaint rates under
# thresholds"); exactly one fails ("Reviewed code... ran regular technical
# sessions" -- pure activity, no keyword, no brag, no result).
#
# A hard filter in generate.py now, not just a soft flag: automated (Zai-
# drafted) generation has no per-job human glance before the resume renders,
# so a bullet that fails this can't ride through on the assumption someone
# will catch it. Whether text "has a brag" is still a real judgment call
# (the 3 no-digit examples above are genuine brags a strict digit-only
# check would wrongly reject) -- the brag-word list is deliberately generous
# to keep the false-exclude rate low, since silently dropping a real
# accomplishment is worse than keeping one weak bullet.
_BRAG_WORDS = re.compile(
    r"\b(cut|cutting|reduce[ds]?|reducing|increase[ds]?|increasing|improve[ds]?|improving|"
    r"prevent(?:ed|s|ing)?|flagged|caught|kept|keeping|letting|enabl(?:ed|ing|es)|"
    r"first|sole|only|led|owned|praised|award(?:ed)?|promoted|saved|savings|"
    r"scal(?:ed|ing)|automat(?:ed|ing))\b", re.I)
_HAS_NUMBER = re.compile(r"\d")

# Rule 4: "no generic verbs like 'Implemented X' with nothing else -- name
# what you designed, optimized, or solved." A bullet opening on one of these
# with no brag signal anywhere in it reads as a bare responsibility, not an
# accomplishment -- checked as a weak OPENER specifically (not banned
# outright), since "Helped design and ship..." followed by a real result is
# fine; it's the bare, brag-free version these catch.
_WEAK_OPENERS = re.compile(
    r"^(implemented|worked on|helped with|responsible for|involved in|assisted with)\b", re.I)


def check_bullet_brag(bullet_text):
    """The half of the rubric that's a property of the bullet's own
    writing, independent of any job posting: does it have a brag (number
    or result word), and does it avoid opening on a bare generic verb with
    nothing else backing it. Unlike "has a keyword" (which depends on what
    a specific JD asks for and so can only be judged per job), this half
    is exactly as true or false the day the point bank entry is written --
    meant to be checked THEN, at the master-doc source, not discovered
    later when a bullet quietly gets cut from some future resume. Returns
    {"has_brag": bool, "weak_opener": bool, "ok": bool}."""
    has_brag = bool(_HAS_NUMBER.search(bullet_text)) or bool(_BRAG_WORDS.search(bullet_text))
    weak_opener = bool(_WEAK_OPENERS.match(bullet_text.strip())) and not has_brag
    return {"has_brag": has_brag, "weak_opener": weak_opener, "ok": has_brag and not weak_opener}


def check_bullet_quality(bullet_text, jd_text, known_skills):
    """Returns {"has_keyword": bool, "has_brag": bool, "weak_opener": bool,
    "ok": bool}. ok is True iff the bullet carries a keyword and/or a brag
    AND doesn't open on a bare generic verb with nothing else backing it --
    the Keyword + Use + Result rule's actual test for whether a bullet
    earns its place on THIS resume. The keyword half is job-specific
    (can't be known until there's a JD to check against) -- see
    check_bullet_brag for the job-independent half of the same rubric,
    checked once at authoring time instead of per job."""
    jd_lower = jd_text.lower()
    has_keyword = any(_contains_skill(bullet_text.lower(), s.lower()) and _contains_skill(jd_lower, s.lower())
                       for s in known_skills)
    brag = check_bullet_brag(bullet_text)
    return {"has_keyword": has_keyword, "has_brag": brag["has_brag"], "weak_opener": brag["weak_opener"],
            "ok": (has_keyword or brag["has_brag"]) and not brag["weak_opener"]}
