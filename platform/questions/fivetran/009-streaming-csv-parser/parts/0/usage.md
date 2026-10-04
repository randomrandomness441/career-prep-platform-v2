Your submission is a single class, no `public` modifier needed (the harness compiles it
alongside its own `Tests.java`, which also defines `LineSource`, `MalformedPolicy`, and
`ParseResult`):

```java
interface LineSource {
    String next(); // null when exhausted
}

enum MalformedPolicy { SKIP_AND_COUNT, FAIL_FAST }

class ParseResult {
    final List<Map<String, String>> records;
    final int malformedCount;
    ParseResult(List<Map<String, String>> records, int malformedCount) {
        this.records = records; this.malformedCount = malformedCount;
    }
}

class Solution {
    static ParseResult parse(LineSource source, MalformedPolicy policy) {
        // your implementation
    }
}
```

A caller wraps a real file without ever loading it whole:

```java
BufferedReader reader = Files.newBufferedReader(path);
LineSource source = () -> { try { return reader.readLine(); } catch (IOException e) { throw new UncheckedIOException(e); } };
ParseResult result = Solution.parse(source, MalformedPolicy.SKIP_AND_COUNT);
```
