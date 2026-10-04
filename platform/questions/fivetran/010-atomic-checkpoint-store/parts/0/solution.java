import java.util.*;

class Solution {
    static void appendAndCommit(Storage store, String currentPath, String tempPath,
                                 long newCursor, List<String> newRecords) {
        Checkpoint current = readCommitted(store, currentPath);
        List<String> merged = new ArrayList<>();
        if (current != null) merged.addAll(current.records);
        merged.addAll(newRecords);
        Checkpoint next = new Checkpoint(newCursor, merged);

        // Only ever write the new content to the temp path -- if this throws, currentPath
        // was never touched. The rename is the one step allowed to affect currentPath,
        // and it's atomic: it either lands fully or (per Storage's contract) doesn't
        // happen at all.
        store.writeFile(tempPath, next.serialize());
        store.renameFile(tempPath, currentPath);
    }

    static Checkpoint readCommitted(Storage store, String currentPath) {
        String content = store.readFile(currentPath);
        return content == null ? null : Checkpoint.parse(content);
    }
}
