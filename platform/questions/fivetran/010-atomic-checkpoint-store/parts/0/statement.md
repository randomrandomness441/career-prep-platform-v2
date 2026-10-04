A connector's checkpoint is a cursor plus the records buffered since the last commit. Both
have to land durably together -- if the process dies while the checkpoint file is being
written, recovery must see either the complete old checkpoint or the complete new one,
never a half-written mix of the two.

Implement:

    static void appendAndCommit(Storage store, String currentPath, String tempPath,
                                 long newCursor, List<String> newRecords)

    static Checkpoint readCommitted(Storage store, String currentPath)

`Storage` has `writeFile(path, content)`, `readFile(path)` (returns `null` if the path
doesn't exist), and `renameFile(from, to)` (atomic, the way a POSIX rename is -- either the
whole file lands at `to` or, if this throws, nothing happens to `to` at all). `writeFile`
is not atomic: if it throws partway, the path it was writing to can be left holding
garbage.

`appendAndCommit` reads whatever's currently committed, appends `newRecords` to it, and
commits the combined checkpoint (new cursor + all records so far) as the new state at
`currentPath`.

**Requirements**

- Never write the new checkpoint straight to `currentPath`. If `writeFile` throws partway
  through that write, `currentPath` must still hold the last good checkpoint, untouched.
- Write the new checkpoint to `tempPath` first, then move it into place with
  `renameFile(tempPath, currentPath)` -- the rename is the only step that's atomic, so
  it's the only step allowed to touch `currentPath`.
- If `appendAndCommit` throws (a simulated crash), the state at `currentPath` must be
  exactly what it was before the call -- callers should be able to call `appendAndCommit`
  again with the same or corrected arguments and pick up cleanly from there.
