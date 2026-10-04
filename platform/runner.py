"""Compile and interrogate a submission.

Two-tier verdict, as chosen:

    REJECTED          did not compile, or failed the correctness tests
    CORRECT_BUT_RACY  tests pass, but TSan or the shaken stress run objected
    CLEAN             tests pass and nothing found a race, deadlock, or hang

The middle tier is the point. A racy solution that happens to produce the right
answer is exactly what LeetCode marks green and what a real reviewer rejects.
"""

import os
import re
import shutil
import subprocess
import tempfile

ROOT = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(ROOT)

WARN = ["-Wall", "-Wextra", "-Wno-unused-parameter", "-Wshadow"]
STD = "c++20"


def _run(cmd, cwd, timeout, env=None):
    e = dict(os.environ)
    if env:
        e.update(env)
    try:
        # errors="replace": a buggy submission can emit arbitrary bytes (garbage
        # read from freed memory), and strict UTF-8 decoding would crash the
        # harness instead of reporting the failure.
        p = subprocess.run(cmd, cwd=cwd, timeout=timeout, env=e,
                           capture_output=True, text=True, errors="replace")
        return p.returncode, p.stdout, p.stderr, False
    except subprocess.TimeoutExpired as ex:
        out = ex.stdout.decode() if isinstance(ex.stdout, bytes) else (ex.stdout or "")
        return 124, out, "", True


TSAN_KEEP = re.compile(r"\.(cpp|hpp|h):\d+")


def _tidy_tsan(text):
    """Strip libc++ trampoline frames from a TSan report so the user's own
    lines are what they see."""
    out, keep = [], False
    for line in text.splitlines():
        if "WARNING: ThreadSanitizer" in line:
            keep = True
            out.append(line)
        elif keep:
            if line.startswith("SUMMARY"):
                out.append(line)
                break
            if re.match(r"^\s*(Write|Read|Previous|Atomic|Location|Mutex|Thread T\d)", line):
                out.append(line)
            elif "#" in line and TSAN_KEEP.search(line) and "/usr/" not in line and "/opt/" not in line:
                out.append("      " + line.strip())
    return "\n".join(out) if out else text[-2000:]


def evaluate(user_code, part_dir, stream=None, quick=False):
    """part_dir must contain tests.cpp. Yields stage dicts through `stream`
    (a callback) and returns the final verdict dict."""

    def emit(stage, status, detail=""):
        row = {"stage": stage, "status": status, "detail": detail}
        if stream:
            stream(row)
        return row

    stages = []
    work = tempfile.mkdtemp(prefix="prep_")
    try:
        with open(os.path.join(work, "solution.hpp"), "w") as f:
            f.write(user_code)
        shutil.copy(os.path.join(part_dir, "tests.cpp"), work)

        inc = ["-I" + work, "-I" + REPO]

        # ── 1. compile ────────────────────────────────────────────────────
        rc, so, se, _ = _run(
            ["clang++", f"-std={STD}", *WARN, "-O1", *inc, "tests.cpp", "-o", "tests"],
            work, 90)
        if rc != 0:
            stages.append(emit("compile", "fail", se[:4000]))
            return {"verdict": "REJECTED", "reason": "compile error", "stages": stages}
        stages.append(emit("compile", "ok"))

        # ── 2. correctness ────────────────────────────────────────────────
        runs, fails, first_fail = 12, 0, ""
        for _ in range(runs):
            rc, so, se, to = _run([os.path.join(work, "tests")], work, 12)
            if to:
                stages.append(emit("tests", "fail", "TIMED OUT — the program stopped making "
                                                   "progress. Deadlock, or waiting on something "
                                                   "that never arrives.\n\n" + so[-1500:]))
                return {"verdict": "REJECTED", "reason": "hang", "stages": stages}
            if rc != 0:
                fails += 1
                if not first_fail:
                    first_fail = (so + se)[-2500:]
        if fails:
            stages.append(emit("tests", "fail",
                               f"failed {fails}/{runs} runs\n\n{first_fail}"))
            return {"verdict": "REJECTED", "reason": "wrong answer", "stages": stages}
        stages.append(emit("tests", "ok", f"{runs}/{runs} runs"))

        if quick:
            return {"verdict": "CLEAN", "reason": "quick run", "stages": stages}

        # ── 3. ThreadSanitizer ────────────────────────────────────────────
        rc, so, se, _ = _run(
            ["clang++", f"-std={STD}", *WARN, "-g", "-O1", "-fno-omit-frame-pointer",
             "-fsanitize=thread", *inc, "tests.cpp", "-o", "tests_tsan"], work, 120)
        if rc != 0:
            stages.append(emit("tsan", "fail", "TSan build failed\n" + se[:2000]))
            return {"verdict": "REJECTED", "reason": "tsan build", "stages": stages}

        racy_detail = ""
        for i in range(20):
            rc, so, se, to = _run([os.path.join(work, "tests_tsan")], work, 25,
                                  env={"TSAN_OPTIONS": "halt_on_error=0 second_deadlock_stack=1"})
            blob = so + se
            if to:
                racy_detail = f"hung under TSan on run {i+1}"
                break
            if "WARNING: ThreadSanitizer" in blob:
                racy_detail = _tidy_tsan(blob)
                break
        if racy_detail:
            stages.append(emit("tsan", "fail", racy_detail))
            stages.append(emit("stress", "skip", "skipped — fix the race first"))
            return {"verdict": "CORRECT_BUT_RACY", "reason": "data race", "stages": stages}
        stages.append(emit("tsan", "ok", "20 runs clean"))

        # ── 4. shaken stress ──────────────────────────────────────────────
        # TSan instrumentation perturbs timing and can smooth over exactly the
        # interleaving that breaks you. This build injects random delays instead.
        rc, so, se, _ = _run(
            ["clang++", f"-std={STD}", *WARN, "-O2", "-DSHAKE", *inc,
             "tests.cpp", "-o", "tests_shake"], work, 90)
        if rc == 0:
            for i in range(150):
                rc, so, se, to = _run([os.path.join(work, "tests_shake")], work, 10)
                if to:
                    stages.append(emit("stress", "fail",
                                       f"HUNG on shaken run {i+1} of 150 — a deadlock that "
                                       f"only appears under unusual scheduling."))
                    return {"verdict": "CORRECT_BUT_RACY", "reason": "deadlock under stress",
                            "stages": stages}
                if rc != 0:
                    stages.append(emit("stress", "fail",
                                       f"failed on shaken run {i+1} of 150\n\n{(so+se)[-2000:]}"))
                    return {"verdict": "CORRECT_BUT_RACY", "reason": "fails under stress",
                            "stages": stages}
            stages.append(emit("stress", "ok", "150 shaken runs clean"))
        else:
            stages.append(emit("stress", "skip", "SHAKE build unavailable"))

        return {"verdict": "CLEAN", "reason": "", "stages": stages}
    finally:
        shutil.rmtree(work, ignore_errors=True)
