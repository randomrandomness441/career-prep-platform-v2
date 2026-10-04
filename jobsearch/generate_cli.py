#!/usr/bin/env python3
"""Thin CLI wrapper so the merged web server -- running under plain system
python3, which doesn't have python-docx -- can trigger real generation by
shelling out to this venv-python script instead of importing generate.py
directly into its own process.

    .venv/bin/python3 generate_cli.py <request.json>

request.json: {"job_id": int, "summary": str, "cover_letter_body": str}
Prints one JSON line to stdout: {"folder": str, "ats": {...}} on success,
{"error": str} on failure (still exit 0 -- the caller checks the "error" key,
not the exit code, so a bad job_id comes back as data, not a stack trace).
"""
import json
import sys

import generate


def main():
    req = json.load(open(sys.argv[1]))
    try:
        result = generate.generate(req["job_id"], req["summary"], req["cover_letter_body"])
    except Exception as e:
        print(json.dumps({"error": str(e)}))
        return
    print(json.dumps({"folder": result["folder"], "ats": result["ats"]}))


if __name__ == "__main__":
    main()
