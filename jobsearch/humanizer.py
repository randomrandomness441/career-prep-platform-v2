"""Deterministic, code-level enforcement of HUMAN_STYLE_RULES
(platform/server.py in the C++ repo) against generated resume/cover-letter
text. Before this, HUMAN_STYLE_RULES was only a comment telling whoever
writes the text (Claude, per job) to follow it -- nothing in code ever
checked. This is the real gate: generate() calls check_human_style() on
both the summary and the cover letter body, and refuses to render past a
violation instead of trusting it was followed.
"""
import re

BANNED_WORDS = ["delve", "leverage", "underscore", "boast", "robust", "seamless"]

# Hedging/filler phrases HUMAN_STYLE_RULES explicitly names. Checked as
# plain substrings on lowercased text -- these are specific enough that a
# false-positive match in real resume/cover-letter prose is vanishingly
# unlikely.
BANNED_PHRASES = [
    "let me know if you have any questions",
    "can be complex",
    "it's worth noting that",
    "it is worth noting that",
    "furthermore", "moreover", "additionally,",
]

# Real em dash, plus the "--" stand-in -- this codebase's own comments use
# "--" constantly, but that's a code-comment convention, not something that
# should ever land in text a human reads as a resume or cover letter.
_DASH_RE = re.compile(r"—|--")

# "it's not just X, it's Y" -- the specific construction HUMAN_STYLE_RULES
# bans by name. Matches "not just ... it's/but" within the same sentence,
# not a bare "not" anywhere (which would false-positive constantly).
_NOT_JUST_RE = re.compile(r"\bnot\s+just\b.{0,60}?\b(it'?s|it\s+is|but)\b", re.I)


def check_human_style(text):
    """Returns a list of plain-English violation descriptions -- empty
    means clean. Call this on every piece of text before it gets rendered;
    treat a non-empty result as a hard stop, not a warning."""
    violations = []
    if _DASH_RE.search(text):
        violations.append("contains an em dash (or a \"--\" stand-in) -- use a period or a comma instead")
    low = text.lower()
    for w in BANNED_WORDS:
        if re.search(rf"\b{re.escape(w)}\b", low):
            violations.append(f'uses the banned word "{w}"')
    for p in BANNED_PHRASES:
        if p in low:
            violations.append(f'uses the banned phrase "{p}"')
    if _NOT_JUST_RE.search(text):
        violations.append('uses the banned "it\'s not just X, it\'s Y" construction')
    return violations
