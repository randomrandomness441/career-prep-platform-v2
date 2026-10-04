## The problem

Design Fivetran itself, at a simplified scale: a platform that pulls data from many
different source services (Salesforce, a SQL database, Stripe, a REST API with its own
pagination and rate limits) and lands it in a customer's destination warehouse, kept
up to date on an ongoing basis.

Cover, in your answer:

- How you model a "connector" so that adding support for a new source doesn't mean
  rewriting the platform -- what's common across every connector, what's specific to
  each one.
- How syncing actually happens on an ongoing schedule: what triggers a sync, how you avoid
  piling up overlapping syncs for the same connector, how a sync resumes after a crash
  instead of starting over.
- How a connector's output schema gets created and kept in sync in the destination when
  the source's schema changes underneath you.
- How failures are isolated: one customer's misbehaving connector (bad credentials,
  a source that's down, a huge backlog) shouldn't degrade the platform for everyone else.
- How this scales horizontally as the number of customers and connectors grows by 100x.

The interviewer will likely add a requirement partway through (a new connector type, a
stricter SLA, a multi-region requirement) to see how much of your design survives it.
There's no code to write here -- answer in plain writing, the way you'd talk it through on
a whiteboard, with a rough architecture diagram in words (boxes and arrows described in
text) if that helps.
