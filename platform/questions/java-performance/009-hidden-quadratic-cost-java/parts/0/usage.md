Your submission is a single class, no `public` modifier needed (the harness compiles it
alongside its own `Tests.java`):

```java
import java.util.ArrayList;
import java.util.List;

class Solution {
    static List<Integer> dedupe(List<Integer> ids) {
        // your implementation
    }
}
```

To profile the given version yourself first, drop it into a standalone class on your
own machine:

```java
import java.util.ArrayList;
import java.util.List;
import java.util.Random;

public class DedupeBench {
    static boolean seenBefore(List<Integer> seen, int id) {
        for (int s : seen) if (s == id) return true;
        return false;
    }
    static List<Integer> dedupe(List<Integer> ids) {
        List<Integer> result = new ArrayList<>();
        for (int id : ids) if (!seenBefore(result, id)) result.add(id);
        return result;
    }
    public static void main(String[] args) {
        List<Integer> ids = new ArrayList<>();
        Random r = new Random(1);
        for (int i = 0; i < 300000; i++) ids.add(r.nextInt(300000));
        long t0 = System.nanoTime();
        List<Integer> out = dedupe(ids);
        System.out.printf("%d unique, %.1fms%n", out.size(), (System.nanoTime() - t0) / 1e6);
    }
}
```

Compile with `javac DedupeBench.java`, then run it under `async-profiler` as shown in
the statement. Come back and fix the real submission above once you've seen it.

## Real flame graphs, already generated

Two real, interactive flame graphs sit next to this file — open either directly in a
browser (drag the file in, or `open dedupe-naive-flamegraph.html`):

- `dedupe-naive-flamegraph.html` — the broken version, profiled for real. `seenBefore`
  dominates inside `dedupe`, exactly as described in this question's reading.
- `dedupe-fixed-flamegraph.html` — the `HashSet`-based fix, profiled under a workload
  heavy enough to also show real G1 garbage collection worker threads as their own
  branch alongside the application code — a live example of question 004's lesson
  (GC work shows up as separate threads, never inside your own call stack).

Both are real async-profiler output, not mockups — click any frame to zoom in, use the
magnifying glass to search for a function by name.

Regenerate them yourself anytime:
```
asprof -d 10 -e cpu -o flamegraph -f out.html <pid>
```

