import java.security.MessageDigest;
import java.nio.charset.StandardCharsets;
import java.util.*;

class Solution {
    private final int virtualNodesPerWorker;
    private final TreeMap<Long, String> ring = new TreeMap<>();

    Solution(List<String> workers, int virtualNodesPerWorker) {
        this.virtualNodesPerWorker = virtualNodesPerWorker;
        for (String w : workers) addWorker(w);
    }

    void addWorker(String worker) {
        for (int i = 0; i < virtualNodesPerWorker; i++) {
            ring.put(hash(worker + "#" + i), worker);
        }
    }

    void removeWorker(String worker) {
        for (int i = 0; i < virtualNodesPerWorker; i++) {
            ring.remove(hash(worker + "#" + i));
        }
    }

    String assign(String key) {
        if (ring.isEmpty()) throw new IllegalStateException("no workers on the ring");
        long h = hash(key);
        Map.Entry<Long, String> e = ring.ceilingEntry(h);
        if (e == null) e = ring.firstEntry(); // wrap around past the largest point
        return e.getValue();
    }

    private static long hash(String s) {
        try {
            MessageDigest md = MessageDigest.getInstance("MD5");
            byte[] digest = md.digest(s.getBytes(StandardCharsets.UTF_8));
            long h = 0;
            for (int i = 0; i < 8; i++) h = (h << 8) | (digest[i] & 0xffL);
            return h;
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }
}
