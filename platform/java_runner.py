"""Compile and run a Java submission. Same verdict contract as runner.py:

    REJECTED  did not compile, or failed the correctness/timing checks
    CLEAN     passed

No TSan/stress equivalent -- this exists for single-threaded performance questions
(the flamegraphs pack's Java content), not concurrency correctness. A concurrent
Java question would need real extra work here; out of scope until one exists.

The submission is a single class named `Solution` (file Solution.java, non-public
so the filename doesn't have to match a public-class name the candidate didn't choose).
`Tests.java` is fixed content living in the question's part_dir, is the public class
with main(), and calls Solution directly (same default package, no import needed).

Java pays real JVM startup + interpreter warm-up cost on every fresh process, unlike a
native binary -- a timing threshold set from a full `time java ...` run (startup
included) is what's actually fair here, not an attempt to isolate "pure algorithm time".
"""

import os
import shutil
import subprocess
import tempfile

ROOT = os.path.dirname(os.path.abspath(__file__))
JAVA_HOME = os.environ.get("JAVA_HOME", "/opt/homebrew/opt/openjdk@21")
JAVAC = os.path.join(JAVA_HOME, "bin", "javac")
JAVA = os.path.join(JAVA_HOME, "bin", "java")


def _run(cmd, cwd, timeout):
    try:
        p = subprocess.run(cmd, cwd=cwd, timeout=timeout, capture_output=True,
                           text=True, errors="replace")
        return p.returncode, p.stdout, p.stderr, False
    except subprocess.TimeoutExpired as ex:
        out = ex.stdout.decode() if isinstance(ex.stdout, bytes) else (ex.stdout or "")
        return 124, out, "", True


def evaluate(user_code, part_dir, stream=None, quick=False):
    """part_dir must contain Tests.java. Yields stage dicts through `stream`
    (a callback) and returns the final verdict dict. `quick` accepted for
    interface parity with runner.evaluate; Java questions have no extra
    non-quick stage yet, so it doesn't change behavior."""

    def emit(stage, status, detail=""):
        row = {"stage": stage, "status": status, "detail": detail}
        if stream:
            stream(row)
        return row

    stages = []
    work = tempfile.mkdtemp(prefix="prep_java_")
    try:
        with open(os.path.join(work, "Solution.java"), "w") as f:
            f.write(user_code)
        shutil.copy(os.path.join(part_dir, "Tests.java"), work)

        # ── 1. compile ────────────────────────────────────────────────────
        rc, so, se, _ = _run([JAVAC, "Solution.java", "Tests.java"], work, 60)
        if rc != 0:
            stages.append(emit("compile", "fail", se[:4000]))
            return {"verdict": "REJECTED", "reason": "compile error", "stages": stages}
        stages.append(emit("compile", "ok"))

        # ── 2. correctness + timing ──────────────────────────────────────
        # Fewer repeats than the C++ harness: JVM startup dominates each run's
        # wall time, so 3 runs is enough to catch flakiness without a slow submit.
        runs, fails, first_fail = 3, 0, ""
        for _ in range(runs):
            rc, so, se, to = _run([JAVA, "-cp", work, "Tests"], work, 30)
            if to:
                stages.append(emit("tests", "fail",
                                   "TIMED OUT -- the program stopped making progress.\n\n" + so[-1500:]))
                return {"verdict": "REJECTED", "reason": "hang", "stages": stages}
            if rc != 0:
                fails += 1
                if not first_fail:
                    first_fail = (so + se)[-2500:]
        if fails:
            stages.append(emit("tests", "fail", f"failed {fails}/{runs} runs\n\n{first_fail}"))
            return {"verdict": "REJECTED", "reason": "wrong answer", "stages": stages}
        stages.append(emit("tests", "ok", f"{runs}/{runs} runs"))

        return {"verdict": "CLEAN", "reason": "", "stages": stages}
    finally:
        shutil.rmtree(work, ignore_errors=True)
