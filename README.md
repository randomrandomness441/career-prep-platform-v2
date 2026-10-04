# Career Prep Platform

Two tools, one small web app, running entirely on your own computer.

## What's inside, in plain words

**1. A coding-interview tutor** (`platform/`)

You get practice problems about writing code that does many things at once
("concurrency" — like a kitchen with several cooks instead of one). You write
your answer, and the app checks it for you: it compiles your code, runs it 50
times on purpose looking for bugs that only show up sometimes (a race between
two cooks reaching for the same pan), and tells you clearly if you passed.
There's also a mock interviewer built in that you can actually talk to.

**2. A job-hunting assistant** (`jobsearch/`)

This one collects real job postings straight from company career pages,
reads each one, and tells you how good a match you are — not by just
counting matching keywords, but by actually reading the posting against
your real work history the way a person would, and explaining why. When
you find one you like, it writes you a tailored resume and cover letter —
picking your strongest, most relevant accomplishments for *that specific
job*, instead of sending the same resume everywhere.

## How the matching and writing actually works

You keep a running list of things you've really done at your jobs — plain
facts, like "I built X and it made Y 90% faster." This is your **point
bank**. For any job posting, the app:

1. Reads the posting
2. Compares it against your point bank
3. Scores how good a fit you are, and explains the real gap (if any)
4. On request, writes a resume and cover letter using *only* facts already
   in your point bank — it's built to refuse to invent anything, and to
   tell you exactly what's missing instead so you can go recall the real
   fact yourself

If the main AI it uses is ever unavailable, it automatically tries a backup
one instead — your resume/cover-letter drafts and job scores don't just
stop working because one provider is having a bad day.

## What's actually in this repo

Just the code. Nothing about *you* is in here.

Everything personal — your name, contact info, the point bank you build,
every job posting it fetches, every resume and cover letter it generates
for you — lives only on your own computer, in files this repo is set up to
never track (see `.gitignore`). Clone this repo and it starts completely
empty: no name, no resume, no job history. You build all of that yourself
the first time you run it, exactly like anyone else would.

## Running it yourself

1. Install Python 3 and [LibreOffice](https://www.libreoffice.org/) (it's
   what turns your resume into a PDF — the app calls its `soffice` command).
2. Install the one Python package this needs:
   ```
   pip install python-docx
   ```
3. Get an API key from [Z.ai](https://z.ai) (this is what reads postings and
   judges your fit) and set it in your terminal:
   ```
   export ZAI_API_KEY=your-key-here
   ```
4. Start the app:
   ```
   python3 platform/server.py
   ```
5. Open `http://localhost:8777` in your browser.
6. Go to the **Profile** tab and either upload a resume or add your real
   accomplishments one at a time — this is what builds *your* point bank.
   Nothing is pre-filled; it's blank until you fill it in.
7. Browse job postings (or add one yourself), and let it judge your fit and
   draft a resume and cover letter whenever you're ready.

No account to create, no cloud service to sign up for. Your data never
leaves your own machine, except for the specific text you choose to send to
the AI for judging or drafting (the job posting plus the relevant point-bank
text — never anything else, and never sent anywhere until you ask for it).
