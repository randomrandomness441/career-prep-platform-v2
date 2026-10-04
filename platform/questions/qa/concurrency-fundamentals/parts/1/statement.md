# What Are the Possible Deadlock Scenarios?

## ELI5: two people, two doorways, both too polite to go first

Two people meet in a narrow doorway from opposite sides. Each steps aside to let the
other through, but they both step to the *same* side, and now they're blocking each
other worse than before. Neither one is doing anything wrong individually; the problem
is that each is waiting for the other to move first, and neither ever will.

## The question

**What are the possible deadlock scenarios in concurrent code, name the patterns, not
just "two threads waiting on each other"?**

A fundamentals question this course's Pure Storage source reported directly. The
interviewer is checking whether you can enumerate *distinct shapes* a deadlock takes,
not just recite the one-sentence definition.
