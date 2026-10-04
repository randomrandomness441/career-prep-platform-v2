# When Does a Concurrent Modification Exception Occur?

## ELI5: rewriting a page while someone's still reading it

Imagine handing someone a page to read, then, while their eyes are still moving down
it, erasing a line and writing something new in its place. They might read the new
line as if it were always there, skip a line entirely, or read a half-erased mess
that's neither the old text nor the new. Nothing about *reading* was wrong, and nothing
about *writing* was wrong on their own, the problem is doing both to the same page at
the same time.

## The question

**When does a "concurrent modification exception" occur, and does the equivalent
mistake exist in C++?**

A fundamentals question this course's Pure Storage source reported directly. The
sharpest version of the answer notes what's actually *different* about C++ here, not
just what the mistake is.
