import java.util.*;

class Checkpoint {
    final long cursor;
    final List<String> records;
    Checkpoint(long cursor, List<String> records) { this.cursor = cursor; this.records = records; }

    String serialize() {
        return cursor + "|" + String.join(",", records);
    }

    static Checkpoint parse(String content) {
        int bar = content.indexOf('|');
        long cursor = Long.parseLong(content.substring(0, bar));
        String rest = content.substring(bar + 1);
        List<String> records = rest.isEmpty() ? new ArrayList<>() : new ArrayList<>(Arrays.asList(rest.split(",")));
        return new Checkpoint(cursor, records);
    }
}

interface Storage {
    void writeFile(String path, String content);
    String readFile(String path);
    void renameFile(String from, String to);
}

// A crash is simulated by arming the store: the very next writeFile() call, to
// whichever path it targets, leaves unparseable garbage behind at that path and then
// throws -- the way a real process dying mid-fwrite() leaves a half-flushed file.
class FakeStorage implements Storage {
    private final Map<String, String> files = new HashMap<>();
    private boolean crashArmed = false;

    void armCrash() { crashArmed = true; }

    public void writeFile(String path, String content) {
        if (crashArmed) {
            crashArmed = false;
            files.put(path, "CORRUPTED-PARTIAL-WRITE");
            throw new RuntimeException("simulated crash mid-write to " + path);
        }
        files.put(path, content);
    }

    public String readFile(String path) { return files.get(path); }

    public void renameFile(String from, String to) {
        String content = files.get(from);
        if (content == null) throw new IllegalStateException("rename source missing: " + from);
        files.remove(from);
        files.put(to, content);
    }
}

public class Tests {
    static int fails = 0;

    static void expect(boolean cond, String name) {
        if (!cond) { System.out.println("FAILED: " + name); fails++; }
    }

    public static void main(String[] args) {
        FakeStorage store = new FakeStorage();

        Solution.appendAndCommit(store, "current", "temp", 10, List.of("a", "b"));
        Checkpoint after1 = Solution.readCommitted(store, "current");
        expect(after1.cursor == 10 && after1.records.equals(List.of("a", "b")),
            "first commit lands cleanly");

        // Simulate a crash during the second commit.
        store.armCrash();
        boolean threw = false;
        try {
            Solution.appendAndCommit(store, "current", "temp", 20, List.of("c"));
        } catch (RuntimeException expected) {
            threw = true;
        }
        expect(threw, "a crash mid-write propagates out of appendAndCommit");

        // The committed checkpoint must be exactly what it was before the crashed
        // attempt -- a torn write anywhere near currentPath breaks this.
        Checkpoint afterCrash;
        try {
            afterCrash = Solution.readCommitted(store, "current");
        } catch (RuntimeException corrupted) {
            System.out.println("FAILED: readCommitted threw after the crash (" + corrupted +
                ") -- currentPath was left holding a torn/unparseable file instead of the old checkpoint");
            fails++;
            afterCrash = null;
        }
        if (afterCrash != null) {
            expect(afterCrash.cursor == 10 && afterCrash.records.equals(List.of("a", "b")),
                "after a crash, currentPath still holds the exact old checkpoint, not a partial new one");
        }

        // A retry after the crash must succeed and merge onto the recovered old state.
        Solution.appendAndCommit(store, "current", "temp", 20, List.of("c"));
        Checkpoint after2 = Solution.readCommitted(store, "current");
        expect(after2.cursor == 20 && after2.records.equals(List.of("a", "b", "c")),
            "retry after the crash commits cleanly and merges onto the recovered state, got cursor="
                + (after2 == null ? "null" : after2.cursor) + " records=" + (after2 == null ? "null" : after2.records));

        if (fails > 0) { System.out.println(fails + " check(s) failed"); System.exit(1); }
        System.out.println("all checks passed");
    }
}
