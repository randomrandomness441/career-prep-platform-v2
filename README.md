# Career Prep Platform

Two tools in one small app. It runs on your own computer, nothing fancy,
nothing in the cloud.

## What's actually in here

**1. A coding-interview tutor** (`platform/`)

You get real practice problems about writing code that does many things at
once. Programmers call this "concurrency." Think of a kitchen with three
cooks instead of one. Things get done faster, but now you have to make sure
two cooks don't grab the same pan at the same time. That's the kind of bug
this teaches you to spot and fix.

You write your answer, and the app checks it for you. It compiles your
code, runs it fifty times on purpose to catch bugs that only show up
sometimes, and tells you plainly whether you passed. There's also a mock
interviewer built in, and you can actually talk to it like a real person.

**2. A job-hunting assistant** (`jobsearch/`)

This part goes out and collects real job postings from company career
pages. It reads each one and tells you how good a match you are. Not by
counting keywords like a dumb filter would. It actually reads the posting
against your real work history, the way a person would, and tells you why.
When you find a job you like, it writes you a resume and cover letter made
just for that job. It picks your strongest, most relevant accomplishments
for that one posting, instead of sending the same resume everywhere like
everybody else does.

## How the matching and writing part works

You keep a running list of things you've really done at your jobs. Plain
facts, nothing fancy. Something like "I built X and it made Y 90% faster."
This list is called your point bank. For any job posting, here's what the
app does:

1. Reads the posting
2. Compares it against your point bank
3. Scores how good a fit you are, and tells you the real gap, if there is one
4. If you ask, writes a resume and cover letter using only facts already in
   your point bank. It's built to refuse to make anything up. If something's
   missing, it tells you exactly what, so you can go remember the real fact
   yourself

If the main AI it talks to ever goes down, it quietly switches to a backup
one. Your scores and drafts don't just stop working because one service is
having a bad day.

## What's actually in this repo, and what isn't

Just the code. Nothing about you is in here, not one bit.

Your name, your contact info, the point bank you build, every job posting
it finds, every resume and cover letter it writes for you. All of that
stays only on your own computer. This repo is set up to never track any of
it (check `.gitignore` if you want to see how). Clone this repo and it
starts completely empty. No name, no resume, no job history. You build all
of that yourself the first time you run it, same as anyone else would.

## Running it yourself

1. Install Python 3 and [LibreOffice](https://www.libreoffice.org/). That's
   what turns your resume into a real PDF. The app just calls its
   `soffice` command for you.
2. Install the one extra Python package this needs:
   ```
   pip install python-docx
   ```
3. Get an API key from [Z.ai](https://z.ai). This is what reads job
   postings and judges your fit. Set it in your terminal:
   ```
   export ZAI_API_KEY=your-key-here
   ```
4. Start the app:
   ```
   python3 platform/server.py
   ```
5. Open `http://localhost:8777` in your browser.
6. Go to the **Profile** tab. Upload a resume, or add your accomplishments
   one at a time. This is what builds your own point bank. Nothing is
   filled in for you. It's blank until you make it yours.
7. Browse job postings, or add one yourself, and let the app judge your fit
   and write you a resume and cover letter whenever you're ready.

No account to make, no cloud service to sign up for. Your data never
leaves your own machine, except for the one thing you actually ask it to
send: the job posting plus the relevant bit of your point bank, sent to
the AI so it can judge or write for you. Nothing more, and only when you
ask for it.
