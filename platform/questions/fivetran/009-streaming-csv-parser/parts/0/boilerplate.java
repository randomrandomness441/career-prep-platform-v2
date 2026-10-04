import java.util.*;

class Solution {
    static ParseResult parse(LineSource source, MalformedPolicy policy) {
        String headerLine = source.next();
        List<Map<String, String>> records = new ArrayList<>();
        if (headerLine == null) return new ParseResult(records, 0);

        String[] header = headerLine.split(",", -1);
        int malformed = 0;
        String line;
        while ((line = source.next()) != null) {
            String[] fields = line.split(",", -1);
            // TODO: always skips and counts a malformed row, never looks at `policy`.
            // FAIL_FAST is supposed to blow up immediately instead of limping along --
            // right now every caller gets SKIP_AND_COUNT behavior whether they asked for
            // it or not.
            if (fields.length != header.length) {
                malformed++;
                continue;
            }
            Map<String, String> row = new LinkedHashMap<>();
            for (int i = 0; i < header.length; i++) row.put(header[i], fields[i]);
            records.add(row);
        }
        return new ParseResult(records, malformed);
    }
}
