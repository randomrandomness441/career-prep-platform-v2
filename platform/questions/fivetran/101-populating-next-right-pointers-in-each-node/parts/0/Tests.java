import java.util.*;

public class Tests {
    static int fails = 0;

    public static void main(String[] args) {
        // 1. empty tree, the 0-node end of the constraint range
        if (Solution.connect(null) != null) {
            fails++;
            System.out.println("FAIL empty tree: expected null");
        }

        // 2. single node at the max value bound
        Node one = new Node(1000);
        Node oneOut = Solution.connect(one);
        if (oneOut != one || one.next != null) {
            fails++;
            System.out.println("FAIL single node: next must stay null");
        }

        // 3. two levels
        check("two levels", "1 # 2 3 #", Solution.connect(tree(1, 2, 3)));

        // 4. three levels, cousin links are required here
        check("three levels", "1 # 2 3 # 4 5 6 7 #",
                Solution.connect(tree(1, 2, 3, 4, 5, 6, 7)));

        // 5. min and max node values
        check("boundary values", "-1000 # -999 1000 #",
                Solution.connect(tree(-1000, -999, 1000)));

        // 6. four levels with duplicate values
        check("four levels", "5 # 5 5 # 1 2 2 1 # 7 7 7 7 7 7 7 7 #",
                Solution.connect(tree(5, 5, 5, 1, 2, 2, 1, 7, 7, 7, 7, 7, 7, 7, 7)));

        // 7. max size: a perfect tree with 4095 nodes over 12 levels
        Node big = Solution.connect(buildPerfect(12));
        int badLevels = audit(big, 12);
        if (badLevels > 0) {
            fails++;
            System.out.println("FAIL deep tree: " + badLevels + " levels had a wrong node count");
        }

        if (fails > 0) {
            System.out.println(fails + " check(s) failed");
            System.exit(1);
        }
        System.out.println("all checks passed");
    }

    static void check(String name, String expected, Node root) {
        String actual = serialize(root);
        if (!expected.equals(actual)) {
            fails++;
            System.out.println("FAIL " + name + ": expected [" + expected + "] got [" + actual + "]");
        }
    }

    // Walks each level by following next chains, which is exactly what
    // those pointers are supposed to support.
    static String serialize(Node root) {
        StringBuilder sb = new StringBuilder();
        Node levelStart = root;
        while (levelStart != null) {
            for (Node cur = levelStart; cur != null; cur = cur.next) {
                sb.append(cur.val).append(' ');
            }
            sb.append("# ");
            levelStart = levelStart.left;
        }
        return sb.toString().trim();
    }

    // Builds a perfect tree in level order from the given values.
    static Node tree(int... vals) {
        if (vals.length == 0) {
            return null;
        }
        Deque<Node> queue = new ArrayDeque<>();
        Node root = new Node(vals[0]);
        queue.add(root);
        int i = 1;
        while (i + 1 < vals.length) {
            Node n = queue.poll();
            n.left = new Node(vals[i++]);
            n.right = new Node(vals[i++]);
            queue.add(n.left);
            queue.add(n.right);
        }
        return root;
    }

    static int counter;

    static Node buildPerfect(int levels) {
        counter = 0;
        return build(levels);
    }

    static Node build(int levelsLeft) {
        if (levelsLeft == 0) {
            return null;
        }
        counter++;
        Node n = new Node((counter * 31) % 2001 - 1000);
        n.left = build(levelsLeft - 1);
        n.right = build(levelsLeft - 1);
        return n;
    }

    // A perfect tree has 2^k nodes on level k. Count them by walking the
    // next chains, so a missing link anywhere breaks the count.
    static int audit(Node root, int levels) {
        int wrong = 0;
        Node levelStart = root;
        int level = 0;
        while (levelStart != null) {
            long count = 0;
            for (Node cur = levelStart; cur != null; cur = cur.next) {
                count++;
            }
            if (count != (1L << level)) {
                wrong++;
            }
            levelStart = levelStart.left;
            level++;
        }
        if (level != levels) {
            wrong++;
        }
        return wrong;
    }
}

class Node {
    public int val;
    public Node left;
    public Node right;
    public Node next;

    public Node() {
    }

    public Node(int _val) {
        val = _val;
    }

    public Node(int _val, Node _left, Node _right, Node _next) {
        val = _val;
        left = _left;
        right = _right;
        next = _next;
    }
}