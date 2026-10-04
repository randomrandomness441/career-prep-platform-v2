# What Is a Mutex?

## ELI5: a single bathroom key on a hook

An office has one bathroom and one key, hanging on a hook by the door. To use the
bathroom, you take the key off the hook. If it's already gone, you wait by the hook
until whoever has it hangs it back up. Only one person can ever be inside at once,
because there's only one key, and nobody can get in without it.

## The question

**What is a mutex, in your own words, and what exactly does it protect?**

This is a fundamentals question asked directly in interviews ("what is a mutex? etc.")
, the interviewer isn't looking for a textbook definition so much as whether you can
say precisely *what invariant* a given mutex is guarding, not just that "it makes
things thread-safe."
