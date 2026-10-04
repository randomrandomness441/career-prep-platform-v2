"""Command-line entry point to edit the point bank / skills without touching
db.py or SQLite directly. Stopgap until the UI (plan step 4) exists.

    python3 manage.py add-skill "Rust" "Languages"
    python3 manage.py list-skills
    python3 manage.py add-point --company LinkedIn --role "Software Engineer" \\
        --start 2024-04 --text "..." --tags Spark,Iceberg
    python3 manage.py list-points
    python3 manage.py retire-point 7

    # manual path: paste a JD the automated fetch didn't cover
    python3 manage.py add-manual-job --company Stripe --title "Staff Eng" \\
        --url https://... --file /tmp/jd.txt

    python3 manage.py list-jobs
    python3 manage.py mark-applied 1 --via referral --referral-name "Priya" \\
        --referral-note "warm intro via Slack"
    python3 manage.py add-status 1 interview --note "onsite loop, 4 rounds"
    python3 manage.py list-applications

    # audit: did the automated fetch miss anything Pinloop/LinkedIn found?
    python3 manage.py compare-external postings.txt   # one "Company - Title" per line
"""

import argparse

import db


def cmd_add_skill(args):
    db.skills_add(args.name, args.category)
    print(f"added/updated skill: {args.name} ({args.category})")


def cmd_list_skills(args):
    by_cat = {}
    for s in db.skills_all():
        by_cat.setdefault(s["category"], []).append(s["name"])
    for cat, names in sorted(by_cat.items()):
        print(f"{cat}: {', '.join(sorted(names))}")


def cmd_add_point(args):
    tags = args.tags.split(",") if args.tags else []
    pid = db.point_bank_add(
        args.company, args.role, args.start, args.end, args.text,
        tags=tags, source_note=args.source_note or "manual entry via manage.py",
    )
    print(f"added point_bank row id={pid}")


def cmd_list_points(args):
    for r in db.point_bank_all(active_only=not args.all):
        flag = "" if r["active"] else " [RETIRED]"
        print(f"[{r['id']}] {r['company']} / {r['role']}{flag}")
        print(f"    {r['text']}")
        print(f"    tags: {', '.join(r['tags'])}")


def cmd_retire_point(args):
    db.point_bank_set_active(args.id, False)
    print(f"retired point_bank row id={args.id}")


def cmd_restore_point(args):
    db.point_bank_set_active(args.id, True)
    print(f"restored point_bank row id={args.id}")


def cmd_add_manual_job(args):
    text = open(args.file).read() if args.file else args.text
    if not text or not text.strip():
        print("nothing to add: pass --file or --text with the JD body")
        return
    jid, is_new = db.jobs_add("manual", args.company, args.title, args.url or "", text.strip())
    if is_new:
        print(f"added job id={jid}")
    else:
        print(f"already tracked as job id={jid} (same company+title+description, or same url)")


def cmd_list_jobs(args):
    for j in db.jobs_list(status=args.status):
        already = " [APPLIED]" if db.applications_already_applied(j["id"]) else ""
        score = f"{j['llm_score']:.0f}" if j["llm_score"] is not None else "-"
        print(f"[{j['id']}] {j['company']:20} {j['title']:45} status={j['status']:10} llm={score}{already}")


def cmd_mark_applied(args):
    db.applications_mark_applied(args.app_id, args.via, referral_name=args.referral_name or "",
                                  referral_note=args.referral_note or "")
    print(f"application {args.app_id} marked applied via {args.via}")


def cmd_add_status(args):
    db.applications_add_status_event(args.app_id, args.status, note=args.note or "")
    print(f"application {args.app_id} -> {args.status}")


def cmd_list_applications(args):
    for a in db.applications_all():
        print(f"[app {a['id']}] job={a['job_id']} {a['company']} / {a['title']}")
        print(f"    status={a['status']}  applied_via={a['applied_via'] or '-'}"
              f"  referral={a['referral_name'] or '-'}")
        print(f"    folder: {a['folder_path']}")
        for ev in a["status_history"]:
            print(f"      {ev['status']:12} {ev['note']}")


def cmd_compare_external(args):
    """Read a plain-text list (one 'Company - Title' per line, e.g. exported
    from Pinloop or copy-pasted off LinkedIn) and report which of those
    postings aren't in our jobs table yet -- the real audit for 'did the
    automated fetch miss something'."""
    known = {(j["company"].strip().lower(), j["title"].strip().lower()) for j in db.jobs_list()}
    missing = []
    with open(args.file) as f:
        for line in f:
            line = line.strip()
            if not line or " - " not in line:
                continue
            company, title = [x.strip() for x in line.split(" - ", 1)]
            if (company.lower(), title.lower()) not in known:
                missing.append((company, title))

    if not missing:
        print("nothing missing -- every listed posting is already tracked")
        return
    print(f"{len(missing)} posting(s) not in our jobs table (fetch these manually or add a slug):")
    for company, title in missing:
        print(f"  {company} - {title}")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)

    s = sub.add_parser("add-skill")
    s.add_argument("name")
    s.add_argument("category")
    s.set_defaults(func=cmd_add_skill)

    s = sub.add_parser("list-skills")
    s.set_defaults(func=cmd_list_skills)

    s = sub.add_parser("add-point")
    s.add_argument("--company", required=True)
    s.add_argument("--role", required=True)
    s.add_argument("--start", required=True, help="YYYY-MM")
    s.add_argument("--end", default=None, help="YYYY-MM, omit if current")
    s.add_argument("--text", required=True)
    s.add_argument("--tags", default="", help="comma-separated skill tags")
    s.add_argument("--source-note", default="")
    s.set_defaults(func=cmd_add_point)

    s = sub.add_parser("list-points")
    s.add_argument("--all", action="store_true", help="include retired rows")
    s.set_defaults(func=cmd_list_points)

    s = sub.add_parser("retire-point")
    s.add_argument("id", type=int)
    s.set_defaults(func=cmd_retire_point)

    s = sub.add_parser("restore-point")
    s.add_argument("id", type=int)
    s.set_defaults(func=cmd_restore_point)

    s = sub.add_parser("add-manual-job")
    s.add_argument("--company", required=True)
    s.add_argument("--title", required=True)
    s.add_argument("--url", default="")
    s.add_argument("--file", default=None, help="path to a text file with the JD body")
    s.add_argument("--text", default=None, help="JD body inline instead of --file")
    s.set_defaults(func=cmd_add_manual_job)

    s = sub.add_parser("list-jobs")
    s.add_argument("--status", default=None)
    s.set_defaults(func=cmd_list_jobs)

    s = sub.add_parser("mark-applied")
    s.add_argument("app_id", type=int)
    s.add_argument("--via", required=True, choices=["company_site", "linkedin", "referral", "recruiter", "other"])
    s.add_argument("--referral-name", default=None)
    s.add_argument("--referral-note", default=None)
    s.set_defaults(func=cmd_mark_applied)

    s = sub.add_parser("add-status")
    s.add_argument("app_id", type=int)
    s.add_argument("status", choices=["phone_screen", "interview", "offer", "rejected", "ghosted", "withdrawn"])
    s.add_argument("--note", default=None)
    s.set_defaults(func=cmd_add_status)

    s = sub.add_parser("list-applications")
    s.set_defaults(func=cmd_list_applications)

    s = sub.add_parser("compare-external")
    s.add_argument("file", help="text file, one 'Company - Title' per line")
    s.set_defaults(func=cmd_compare_external)

    args = p.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
