import java.util.*;

class Solution {
    static void appendAndCommit(Storage store, String currentPath, String tempPath,
                                 long newCursor, List<String> newRecords) {
        Checkpoint current = readCommitted(store, currentPath);
        List<String> merged = new ArrayList<>();
        if (current != null) merged.addAll(current.records);
        merged.addAll(newRecords);
        Checkpoint next = new Checkpoint(newCursor, merged);
        // TODO: writes straight to currentPath -- no staging file, no rename. If the
        // write throws partway (a simulated crash), currentPath is left holding whatever
        // got flushed before the crash instead of the last good checkpoint. There's no
        // atomic step here at all, just a single non-atomic write to the real file.
        store.writeFile(currentPath, next.serialize());
    }

    static Checkpoint readCommitted(Storage store, String currentPath) {
        String content = store.readFile(currentPath);
        return content == null ? null : Checkpoint.parse(content);
    }
}
