"""Render a resume and cover letter to .docx, deliberately ATS-safe.

Real ATS parsers frequently fail on tables, text boxes, multi-column layouts,
headers/footers holding real content, and embedded graphics -- the text
inside them either gets scrambled or dropped entirely. This renderer uses
only plain paragraphs, run-level bold/italic, explicit tab stops, and a
literal bullet character -- never Word's native numbered-list XML (ATS
parsers occasionally mishandle it) -- so a parser reads it top to bottom
exactly as intended.

Layout is deliberately tight (small margins, minimal paragraph spacing,
single line-spacing) -- this is the FIRST lever for fitting one page, not
cutting real content. Word's own defaults (1"-1.25" margins, ~8-10pt
space-after on every paragraph, blank-paragraph spacers between sections)
were eating a real page's worth of space on their own: the same 12-bullet,
2-role content that rendered to 2 pages under the old defaults fits
comfortably on 1 under this layout with EVERY bullet still included --
proven by actually rendering and checking the real page count (pdfinfo),
not assumed. generate.py's bullet-count trimming is the fallback for when
even this tight a layout still overflows, not the first move.
"""

from datetime import datetime

from docx import Document
from docx.shared import Pt, Inches
from docx.enum.text import WD_TAB_ALIGNMENT
from docx.oxml import OxmlElement
from docx.oxml.ns import qn

PAGE_WIDTH = Inches(8.5)
MARGIN_TOP_BOTTOM = Inches(0.55)
MARGIN_LEFT_RIGHT = Inches(0.65)
USABLE_WIDTH = PAGE_WIDTH - MARGIN_LEFT_RIGHT * 2   # where a right-aligned tab stop lands

BODY_SIZE = Pt(10.5)
SECTION_SPACE_BEFORE = 9     # points -- plain numbers throughout this file, see _tighten
SECTION_SPACE_AFTER = 3
LINE_SPACE_AFTER = 1

BULLET_INDENT = Pt(14)       # hanging indent: wrapped lines of a long bullet
BULLET_CHAR = "•\t"     # align under the text, not back under the bullet glyph


def _tighten(paragraph, space_before=0, space_after=LINE_SPACE_AFTER, line_spacing=1.0):
    """space_before/space_after are always plain point numbers here, wrapped
    into Pt() exactly once, in exactly one place. A real bug used to live
    here: _heading() passed an already-Pt()-wrapped constant for
    space_before, which got wrapped a SECOND time into Pt(Pt(8)) == a
    ~1400-inch gap (Pt() treats its argument as a point count, and a Length
    object's int value IS its EMU count, not its point count); several
    other call sites passed a bare int for space_after, which python-docx
    silently drops (it isn't a Length, so no spacing attribute is written
    at all, and the paragraph falls back to the template's own default
    instead of the tight value the code asked for). Both explain spacing
    that looked "random" per heading/paragraph rather than uniformly tight."""
    fmt = paragraph.paragraph_format
    fmt.space_before = Pt(space_before)
    fmt.space_after = Pt(space_after)
    fmt.line_spacing = line_spacing


def _set_margins(doc):
    s = doc.sections[0]
    s.top_margin = s.bottom_margin = MARGIN_TOP_BOTTOM
    s.left_margin = s.right_margin = MARGIN_LEFT_RIGHT


def _bottom_border(paragraph, size=4, color="999999"):
    """A thin rule under a section heading -- plain XML paragraph border,
    not a drawn shape or table, so it adds zero risk to ATS text
    extraction (the text content is untouched) while reading as a real
    section break instead of just bold text sitting in the body font."""
    pPr = paragraph._p.get_or_add_pPr()
    pBdr = OxmlElement("w:pBdr")
    bottom = OxmlElement("w:bottom")
    bottom.set(qn("w:val"), "single")
    bottom.set(qn("w:sz"), str(size))   # eighths of a point
    bottom.set(qn("w:space"), "2")
    bottom.set(qn("w:color"), color)
    pBdr.append(bottom)
    pPr.append(pBdr)


def _heading(doc, text):
    h = doc.add_paragraph()
    h.add_run(text).bold = True
    _tighten(h, space_before=SECTION_SPACE_BEFORE, space_after=SECTION_SPACE_AFTER)
    _bottom_border(h)
    return h


def _bullet(doc, text):
    bp = doc.add_paragraph()
    bp.paragraph_format.left_indent = BULLET_INDENT
    bp.paragraph_format.first_line_indent = -BULLET_INDENT
    bp.paragraph_format.tab_stops.add_tab_stop(BULLET_INDENT, WD_TAB_ALIGNMENT.LEFT)
    bp.add_run(BULLET_CHAR + text)
    _tighten(bp)
    return bp


def render(profile, title, summary, roles, skills_by_category, out_path):
    """roles: [{"company", "role", "dates", "bullets": [str, ...]}, ...]
    skills_by_category: {category: [name, ...]}"""
    doc = Document()
    _set_margins(doc)

    style = doc.styles["Normal"]
    style.font.name = "Calibri"
    style.font.size = BODY_SIZE

    name_p = doc.add_paragraph()
    name_run = name_p.add_run(profile["name"])
    name_run.bold = True
    name_run.font.size = Pt(17)
    _tighten(name_p, space_after=0)

    title_p = doc.add_paragraph(title)
    _tighten(title_p, space_after=2)

    contact = doc.add_paragraph()
    contact.add_run(
        f"{profile['phone']} | {profile['email']} | {profile['linkedin']} | {profile['github']}"
    )
    _tighten(contact, space_after=2)

    _heading(doc, "SUMMARY")
    _tighten(doc.add_paragraph(summary))

    _heading(doc, "EXPERIENCE")
    for role in roles:
        rp = doc.add_paragraph()
        rp.paragraph_format.tab_stops.add_tab_stop(USABLE_WIDTH, WD_TAB_ALIGNMENT.RIGHT)
        rp.add_run(role["role"]).bold = True
        rp.add_run(f"\t{role['dates']}")
        _tighten(rp, space_after=0)
        cp = doc.add_paragraph()
        cp.add_run(role["company"]).italic = True
        _tighten(cp, space_after=2)
        for bullet in role["bullets"]:
            _bullet(doc, bullet)

    _heading(doc, "SKILLS")
    for category, names in skills_by_category.items():
        sp = doc.add_paragraph()
        sp.add_run(f"{category}: ").bold = True
        sp.add_run(", ".join(names))
        _tighten(sp)

    _heading(doc, "EDUCATION")
    _tighten(doc.add_paragraph(profile["education"]))

    doc.save(out_path)
    return out_path


def render_cover_letter_docx(profile, company, role_title, body_text, out_path):
    """A real business-letter shape -- date, recipient, "Re:" line,
    salutation, body, sign-off -- not just the LLM's body paragraphs
    dropped under a name and contact line with nothing else. None of this
    added chrome passes through humanizer (it isn't LLM-authored, nothing
    here needs the style gate), only body_text ever did, same as before."""
    doc = Document()
    _set_margins(doc)
    style = doc.styles["Normal"]
    style.font.name = "Calibri"
    style.font.size = Pt(11)

    name_p = doc.add_paragraph()
    name_p.add_run(profile["name"]).bold = True
    _tighten(name_p, space_after=0)
    contact = doc.add_paragraph(f"{profile['email']} | {profile['phone']}")
    _tighten(contact, space_after=16)

    date_p = doc.add_paragraph(datetime.now().strftime("%B %d, %Y"))
    _tighten(date_p, space_after=10)

    recipient_p = doc.add_paragraph(f"Hiring Team, {company}")
    _tighten(recipient_p, space_after=2)
    re_p = doc.add_paragraph(f"Re: {role_title}")
    _tighten(re_p, space_after=14)

    salutation_p = doc.add_paragraph("Dear Hiring Manager,")
    _tighten(salutation_p, space_after=10)

    paras = [p.strip() for p in body_text.split("\n\n") if p.strip()]
    for para in paras:
        p = doc.add_paragraph(para)
        _tighten(p, space_after=10, line_spacing=1.1)

    sign_off = doc.add_paragraph("Sincerely,")
    _tighten(sign_off, space_before=10, space_after=2)
    sign_name = doc.add_paragraph(profile["name"])
    _tighten(sign_name, space_after=0)

    doc.save(out_path)
    return out_path
