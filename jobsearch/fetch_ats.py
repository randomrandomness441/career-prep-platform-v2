"""Pull postings from official, free, public APIs -- Greenhouse, Lever,
Ashby. No key, no login, no scraping. Verified this session that all three
are genuinely open, unauthenticated endpoints meant for building careers
pages, not paywalled:
  https://developers.greenhouse.io  (boards-api.greenhouse.io)
  https://github.com/lever/postings-api  (api.lever.co)
  https://developers.ashbyhq.com/docs/public-job-posting-api  (api.ashbyhq.com)

Target companies live in companies.py, not here -- edit that file to add or
fix a slug. Every posting goes through db.jobs_add(), which deduplicates by
url and by content hash, so re-running this on a schedule is safe: already-
seen postings come back as (existing_id, is_new=False), nothing doubles up.
"""

import datetime
import html
import json
import re
import sys
import time
import urllib.error
import urllib.request

import db
from companies import COMPANIES

# Fetch-time filter, applied before a posting ever reaches the db: only
# India/Bangalore/Bengaluru (incl. "Remote - India"), only posted within
# the last month. Sourav's own words: "fetch the jobs by location or date,
# maximum can be a month, thats it" -- don't clutter the db with the other
# ~95% of each board (mostly US/EU postings) until there's a reason to look
# at them. Title is deliberately NOT filtered here -- that's the level /
# "engineering roles only" controls in the UI, a different job.
_INDIA_WORDS = ("india", "bangalore", "bengaluru")
_INDIA_RE = re.compile(r"\b(" + "|".join(_INDIA_WORDS) + r")\b", re.IGNORECASE)
MAX_POSTING_AGE_DAYS = 30


def is_target_location(location, description=""):
    # \b word-boundary match -- a plain substring check would match
    # "Indiana" on "india", a real false positive hit in fetched data.
    if location and _INDIA_RE.search(location):
        return True
    if not location and description:
        return bool(_INDIA_RE.search(description[:500]))
    return False


def _get(url):
    req = urllib.request.Request(url, headers={"User-Agent": "jobsearch/1.0 (personal use)"})
    with urllib.request.urlopen(req, timeout=15) as resp:
        return json.loads(resp.read().decode())


def _clean_html(raw):
    # unescape twice: some boards' JSON has double-encoded entities
    # ("&amp;nbsp;"), so a single pass leaves literal "&nbsp;" text sitting
    # in the JD instead of a real space -- visible once the detail panel's
    # JD box was actually made readable instead of a cramped scrollbox.
    text = html.unescape(html.unescape(raw or ""))
    text = re.sub(r"<[^<]+?>", "\n", text)
    return re.sub(r"\n{3,}", "\n\n", text).strip()


def _parse_iso(s):
    if not s:
        return None
    try:
        return datetime.datetime.fromisoformat(s).timestamp()
    except ValueError:
        return None


def fetch_greenhouse(slug):
    data = _get(f"https://boards-api.greenhouse.io/v1/boards/{slug}/jobs?content=true")
    out = []
    for j in data.get("jobs", []):
        out.append({
            "title": j["title"],
            "url": j.get("absolute_url", ""),
            "description_raw": _clean_html(j.get("content", "")),
            "location": (j.get("location") or {}).get("name", ""),
            # first_published, not updated_at -- updated_at moves every time
            # a company edits an old listing, which would wrongly keep a
            # months-old posting under a "posted within 30 days" cutoff.
            "posted_ts": _parse_iso(j.get("first_published") or j.get("updated_at")),
        })
    return out


def fetch_lever(slug):
    data = _get(f"https://api.lever.co/v0/postings/{slug}?mode=json")
    out = []
    for j in data:
        desc = j.get("descriptionPlain") or _clean_html(j.get("description", ""))
        created = j.get("createdAt")  # epoch milliseconds
        out.append({"title": j.get("text", ""), "url": j.get("hostedUrl", ""), "description_raw": desc,
                     "location": (j.get("categories") or {}).get("location", ""),
                     "posted_ts": created / 1000 if created else None})
    return out


def fetch_ashby(slug):
    data = _get(f"https://api.ashbyhq.com/posting-api/job-board/{slug}")
    out = []
    for j in data.get("jobs", []):
        desc = j.get("descriptionPlain") or _clean_html(j.get("descriptionHtml", ""))
        out.append({"title": j.get("title", ""), "url": j.get("jobUrl", ""), "description_raw": desc,
                     "location": j.get("location", ""),
                     "posted_ts": _parse_iso(j.get("publishedAt"))})
    return out


FETCHERS = {"greenhouse": fetch_greenhouse, "lever": fetch_lever, "ashby": fetch_ashby}


def main():
    total_new, total_dup, total_err, total_skip_loc, total_skip_age = 0, 0, 0, 0, 0
    cutoff = time.time() - MAX_POSTING_AGE_DAYS * 86400
    for name, ats, slug in COMPANIES:
        try:
            postings = FETCHERS[ats](slug)
        except urllib.error.HTTPError as e:
            print(f"[{ats:10}] {name:20} slug '{slug}' -> HTTP {e.code} (wrong slug, or board closed)")
            total_err += 1
            continue
        except Exception as e:
            print(f"[{ats:10}] {name:20} slug '{slug}' -> ERROR {e}")
            total_err += 1
            continue

        new_here, dup_here, skip_loc, skip_age = 0, 0, 0, 0
        for p in postings:
            if not p["description_raw"]:
                continue
            if not is_target_location(p.get("location", ""), p["description_raw"]):
                skip_loc += 1
                continue
            # A posting whose date we couldn't parse is kept rather than
            # silently dropped -- we can't confirm it's stale, and staying
            # silent about a parse failure is worse than an occasional old
            # row slipping through.
            posted_ts = p.get("posted_ts")
            if posted_ts is not None and posted_ts < cutoff:
                skip_age += 1
                continue
            _, is_new = db.jobs_add(ats, name, p["title"], p["url"], p["description_raw"],
                                     p.get("location", ""), posted_ts)
            if is_new:
                new_here += 1
            else:
                dup_here += 1
        total_new += new_here
        total_dup += dup_here
        total_skip_loc += skip_loc
        total_skip_age += skip_age
        print(f"[{ats:10}] {name:20} {len(postings):4} postings -> {new_here} new, {dup_here} already tracked, "
              f"{skip_loc} outside India, {skip_age} older than {MAX_POSTING_AGE_DAYS}d")
        time.sleep(0.5)  # be polite, not just fast

    print(f"\ntotal: {total_new} new job rows added, {total_dup} already-tracked duplicates skipped, "
          f"{total_skip_loc} skipped (outside India, never saved), "
          f"{total_skip_age} skipped (older than {MAX_POSTING_AGE_DAYS}d, never saved), "
          f"{total_err} sources errored")


if __name__ == "__main__":
    main()
