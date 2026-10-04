"""Tailor a resume to one job: select the subset of point-bank bullets that
maximizes the real ATS score for this specific posting, then export +
re-score against ats.py so the improvement over the untailored baseline is
measured, not asserted.

The summary line and cover letter body are NOT auto-templated here -- those
are written per job, grounded in the point bank and the posting's real
specifics, under HUMAN_STYLE_RULES (see platform/server.py in the C++ repo
for the original ruleset this reuses) -- and humanizer.check_human_style()
is a real, code-level gate on both before anything renders, not just a
comment telling whoever writes the text to follow the rules. A generic
script writing "I am excited to apply..." is exactly the AI-slop shape this
project exists to avoid; a person (or me, deliberately, per job) writing
from real facts is not something a template should try to shortcut.
"""

import difflib
import os
import re
import shutil
import subprocess
import time

import ats
import db
import humanizer
import resume_render

# Same threshold and same difflib.SequenceMatcher math as
# db.point_bank_find_similar (already proven: catches real near-duplicate
# bullets, like one point edited into a second slightly-different version
# with the old one left active by mistake -- found exactly that pair in the
# real point bank while designing this). Character/sequence similarity, not
# semantic -- cheap, no API cost, and matches the actual failure pattern
# seen so far (an edited duplicate), not a hypothetical "differently-worded
# same fact" case there's no evidence of yet.
DUPLICATE_THRESHOLD = 0.82


def _cluster_duplicates(rows, threshold=DUPLICATE_THRESHOLD):
    """Groups near-duplicate point-bank rows so selection only ever lets
    ONE member of a cluster onto a given resume -- two phrasings of the
    same real accomplishment can't both occupy bullet slots, and can't
    double-count toward keyword coverage (which is exactly the risk: if
    both happen to contain the same keyword, the scorer would credit it
    twice for one real piece of evidence). Returns {row_id: cluster_id}."""
    clusters = []  # list of representative-normalized-text per cluster
    assigned = {}
    for r in rows:
        norm = re.sub(r"\s+", " ", r["text"].strip().lower())
        placed = False
        for ci, rep_norm in enumerate(clusters):
            if difflib.SequenceMatcher(None, norm, rep_norm).ratio() >= threshold:
                assigned[r["id"]] = ci
                placed = True
                break
        if not placed:
            clusters.append(norm)
            assigned[r["id"]] = len(clusters) - 1
    return assigned

MAX_PAGES = 1
# Hard ceiling on the greedy search below (bullets considered per step) --
# the point bank is expected to grow well past the ~30 rows it has today
# ("100 points" was the real number mentioned), and re-scoring every
# remaining candidate every step is O(n^2) in bullet count. 60 is far more
# than a one-page resume could ever fit, so this never actually binds in
# practice -- it's a ceiling against a future point bank in the hundreds,
# not a meaningful cap today.
MAX_CANDIDATES = 60


def select_bullets_max_ats(job, resume_title, summary, skill_names, skills_line_text, candidate_years):
    """Ranks every real point-bank bullet by its marginal contribution to
    ats.score_resume() for THIS job, then includes ALL of them by default
    -- not just whichever small subset first maxes out the score.

    An earlier version stopped as soon as no remaining bullet raised the
    score further, which sounds right but isn't: score_resume() rewards
    matched-skill COVERAGE, so once every skill the JD asks for already has
    a strong bullet behind it, adding more real bullets (different
    accomplishments, same skills, or skills this JD doesn't ask for) can't
    move the number -- so the greedy search plateaued at 2-3 bullets out of
    15 real ones. That's "ATS-optimal" by the narrow metric and a resume
    no human reviewer would take seriously: it throws away most of a real
    body of work to chase a score that was already maxed. Checked directly
    (rendered the full 15-bullet set through the tight layout in
    resume_render.py and confirmed via pdfinfo): it fits one page anyway,
    so there was never a real reason to cut.

    So: keep going past the plateau, ranking what's left by the same
    marginal-value math (ties or small drops included) instead of
    stopping. Every real bullet ends up in the resume, ordered by real
    ATS contribution -- highest-value first in `selection_order`, which is
    exactly the order generate()'s one-page trim (a real fallback for when
    the point bank eventually grows past what one page can hold, not the
    normal case today) cuts from the back of.

    Returns (roles, selection_order, final_score). roles is already
    grouped/ordered the way resume_render expects, each role's own bullets
    kept in natural point-bank order (not selection order), so the resume
    still reads as a coherent role narrative, not a relevance-sorted
    jumble."""
    jd_text = job["description_raw"]
    rows = [r for r in db.point_bank_all() if not r["text"].lstrip().startswith("[")][:MAX_CANDIDATES]
    cluster_of = _cluster_duplicates(rows)

    selection_order = []   # in the order chosen -- first = highest marginal value
    remaining = list(rows)
    best_score = -1
    while remaining:
        best_row, best_trial_score = None, -1
        for r in remaining:
            trial_text = "\n".join(x["text"] for x in selection_order + [r])
            trial = ats.score_resume(resume_title, summary, trial_text, skills_line_text,
                                      jd_text, skill_names, candidate_years)
            if trial["score"] > best_trial_score:
                best_trial_score, best_row = trial["score"], r
        selection_order.append(best_row)
        best_score = max(best_score, best_trial_score)
        # Drop every near-duplicate of the row just picked, not just the
        # row itself -- a second phrasing of the same accomplishment has
        # no real marginal value left to offer once its cluster-mate is in.
        picked_cluster = cluster_of[best_row["id"]]
        remaining = [r for r in remaining if cluster_of[r["id"]] != picked_cluster]

    return _group_into_roles(selection_order), selection_order, best_score


def _group_into_roles(selected_rows):
    """selected_rows: point-bank rows in selection order (highest marginal
    value first). Re-groups by (company, role), each role's own bullets
    kept in natural point-bank order (not selection order) so a role still
    reads top-to-bottom the way it actually happened, and roles ordered
    most-recent-first the way point_bank_all() already sorts."""
    by_role = {}
    seen_keys = []
    for r in selected_rows:
        key = (r["company"], r["role"])
        if key not in seen_keys:
            seen_keys.append(key)
        by_role.setdefault(key, []).append(r)

    all_rows_by_id = {r["id"]: r for r in db.point_bank_all()}
    roles = []
    for key in seen_keys:
        items = sorted(by_role[key], key=lambda r: r["id"])
        company, role = key
        end = items[0]["end_date"] or "Present"
        roles.append({
            "company": company,
            "role": role,
            "dates": f"{items[0]['start_date']} - {end}",
            "bullets": [r["text"] for r in items],
            "_row_ids": [r["id"] for r in items],  # internal -- lets the one-page trim below drop by id
        })
    return roles


def _page_count(pdf_path):
    out = subprocess.run(["pdfinfo", pdf_path], capture_output=True, text=True, timeout=15)
    for line in out.stdout.splitlines():
        if line.startswith("Pages:"):
            return int(line.split(":")[1].strip())
    return None


def _render_and_count(profile, resume_title, summary, roles, skills_by_category, folder):
    docx_path = os.path.join(folder, "resume.docx")
    pdf_path = os.path.join(folder, "resume.pdf")
    resume_render.render(profile, resume_title, summary, roles, skills_by_category, docx_path)
    subprocess.run(["soffice", "--headless", "--convert-to", "pdf", "--outdir", folder, docx_path],
                    capture_output=True, timeout=60)
    return docx_path, pdf_path, _page_count(pdf_path)


def _lowest_value_row_id(roles, selection_order):
    """The single bullet to drop when the resume runs over a page: the
    last one the greedy selection above added, i.e. the smallest marginal
    ATS-score contribution of everything currently included."""
    present_ids = {rid for role in roles for rid in role["_row_ids"]}
    for row in reversed(selection_order):
        if row["id"] in present_ids:
            return row["id"]
    return None


def _drop_row(roles, row_id):
    out = []
    for role in roles:
        bullets = [(b, rid) for b, rid in zip(role["bullets"], role["_row_ids"]) if rid != row_id]
        if bullets:
            role = dict(role)
            role["bullets"] = [b for b, _ in bullets]
            role["_row_ids"] = [rid for _, rid in bullets]
            out.append(role)
    return out


def generate(job_id, summary, cover_letter_body, out_root="data/applications"):
    violations = humanizer.check_human_style(summary) + humanizer.check_human_style(cover_letter_body)
    if violations:
        raise ValueError("Humanizer check failed, nothing generated:\n- " + "\n- ".join(violations))

    job = db.jobs_get(job_id)
    profile = db.profile_all()
    skills = db.skills_all()
    skill_names = [s["name"] for s in skills]
    skills_line_text = ", ".join(skill_names)
    candidate_years = float(profile.get("years_experience", 0) or 0)
    resume_title = profile["title"]

    # A regenerate (new summary/cover letter for a job already drafted but
    # never actually submitted) replaces that draft rather than stacking a
    # second application row for the same job -- applications_add always
    # inserts, nothing dedups by job_id on its own. A job already past
    # 'generated' (really applied) is left alone; this never touches that,
    # it only ever replaces an un-submitted draft.
    #
    # Done FIRST, before any new file gets written: the folder name is
    # slug-plus-date, so a same-day regenerate for the same job produces
    # the exact same path as the draft being replaced. Doing this cleanup
    # AFTER rendering (the original order) deleted the brand-new files
    # along with the old ones, since they shared a path -- caught by
    # actually regenerating the same job twice in one day and watching the
    # output folder vanish, not by reasoning about it.
    old_draft = db.applications_find_draft(job_id)
    if old_draft:
        db.applications_delete(old_draft["id"])
        if old_draft["folder_path"] and os.path.isdir(old_draft["folder_path"]):
            shutil.rmtree(old_draft["folder_path"])

    roles, selection_order, _ = select_bullets_max_ats(
        job, resume_title, summary, skill_names, skills_line_text, candidate_years)

    # Keyword + Use + Result rubric, now a hard filter: a bullet with
    # neither a JD keyword nor a brag (or that opens on a bare "Implemented
    # X" with nothing else) doesn't make the resume. This used to be a soft
    # flag written into ats_estimate.md for a human to glance at -- upgraded
    # to a real exclusion because automated (Zai-drafted) generation has no
    # per-job human review step before the resume renders, so a weak bullet
    # can't ride through on the assumption someone will catch it.
    cut_bullets = []
    kept_roles = []
    for role in roles:
        kept = []
        for bullet, rid in zip(role["bullets"], role["_row_ids"]):
            q = ats.check_bullet_quality(bullet, job["description_raw"], skill_names)
            if q["ok"]:
                kept.append((bullet, rid))
            else:
                cut_bullets.append(bullet)
        if kept:
            role = dict(role)
            role["bullets"] = [b for b, _ in kept]
            role["_row_ids"] = [rid for _, rid in kept]
            kept_roles.append(role)
    roles = kept_roles
    selection_order = [r for r in selection_order if r["text"] not in cut_bullets]

    skills_by_category = {}
    for s in skills:
        skills_by_category.setdefault(s["category"], []).append(s["name"])

    slug = re.sub(r"[^a-zA-Z0-9]+", "-", f"{job['company']}-{job['title']}").strip("-").lower()
    folder = os.path.join(out_root, f"{slug}-{time.strftime('%Y-%m-%d')}")
    os.makedirs(folder, exist_ok=True)

    # One-page enforcement: render, check the REAL rendered page count (not
    # a guessed bullet budget), and if it's over, drop the single lowest-
    # marginal-ATS-value bullet and re-render. Repeats until it fits or
    # there's nothing left to drop -- bounded by len(roles' bullets), so
    # this always terminates.
    docx_path = pdf_path = None
    pages = None
    for _ in range(len(selection_order) + 1):
        docx_path, pdf_path, pages = _render_and_count(
            profile, resume_title, summary, roles, skills_by_category, folder)
        if pages is None or pages <= MAX_PAGES:
            break
        drop_id = _lowest_value_row_id(roles, selection_order)
        if drop_id is None:
            break
        roles = _drop_row(roles, drop_id)
    one_page_ok = pages is not None and pages <= MAX_PAGES

    final_bullets_text = "\n".join(b for role in roles for b in role["bullets"])
    result = ats.score_resume(resume_title, summary, final_bullets_text, skills_line_text,
                               job["description_raw"], skill_names, candidate_years)
    result["rendered_pages"] = pages
    result["one_page_ok"] = one_page_ok

    # cut_bullets was computed above, before this resume was ever rendered --
    # these never made it onto the page at all (see the hard filter right
    # after select_bullets_max_ats). Reported here purely for visibility:
    # a real accomplishment that got cut for having no keyword/brag is worth
    # knowing about, in case it's worth rewriting in the point bank.
    result["weak_bullets"] = cut_bullets

    cover_docx = os.path.join(folder, "cover_letter.docx")
    resume_render.render_cover_letter_docx(profile, job["company"], job["title"], cover_letter_body, cover_docx)
    subprocess.run(["soffice", "--headless", "--convert-to", "pdf", "--outdir", folder, cover_docx],
                    capture_output=True, timeout=60)
    cover_pdf = os.path.join(folder, "cover_letter.pdf")

    with open(os.path.join(folder, "job_description.md"), "w") as f:
        f.write(f"# {job['title']} — {job['company']}\n\n{job['url']}\n\n{job['description_raw']}\n")

    with open(os.path.join(folder, "ats_estimate.md"), "w") as f:
        f.write(f"# ATS estimate: {result['score']}/100\n\n")
        f.write(f"Rendered pages: {pages} {'(within the 1-page target)' if one_page_ok else '(OVER 1 page -- ran out of bullets to trim)'}\n\n")
        f.write(f"Matched: {', '.join(result['matched']) or '(none)'}\n\n")
        f.write(f"Missing: {', '.join(result['missing']) or '(none)'}\n\n")
        if result.get("experience_penalty"):
            f.write(f"Experience gap: JD asks for {result['years_required']}+ years, "
                    f"candidate has {candidate_years} -- score docked {result['experience_penalty']} pts\n\n")
        f.write(f"Breakdown: {result['breakdown']}\n\n")
        if result["weak_bullets"]:
            f.write("Cut from this resume (no JD keyword, no brag, or a bare generic-verb "
                     "opener -- real point-bank entries worth rewriting if you want them back):\n")
            for b in result["weak_bullets"]:
                f.write(f"- {b}\n")
        else:
            f.write("Every bullet on this resume carries a keyword and/or a brag.\n")

    db.applications_add(job_id, pdf_path, cover_pdf, result, folder)
    db.jobs_set_status(job_id, "generated")

    return {"folder": folder, "ats": result, "roles": roles,
            "resume_docx": docx_path, "resume_pdf": pdf_path,
            "cover_docx": cover_docx, "cover_pdf": cover_pdf}
