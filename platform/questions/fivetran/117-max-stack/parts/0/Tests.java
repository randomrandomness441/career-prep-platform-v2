import java.util.*;

public class Tests {

    static int fails = 0;

    static void check(String label, int expected, int actual) {
        if (expected != actual) {
            System.out.println("FAIL " + label + ": expected " + expected + " but got " + actual);
            fails++;
        }
    }

    public static void main(String[] args) {
        // Test 1: canonical LeetCode example
        Solution s1 = new Solution();
        s1.push(5);
        s1.push(1);
        s1.push(5);
        check("t1 top", 5, s1.top());
        check("t1 popMax", 5, s1.popMax());
        check("t1 top after popMax", 1, s1.top());
        check("t1 peekMax", 5, s1.peekMax());
        check("t1 pop", 1, s1.pop());
        check("t1 final top", 5, s1.top());

        // Test 2: single element, the trivial case
        Solution s2 = new Solution();
        s2.push(-7);
        check("t2 top", -7, s2.top());
        check("t2 peekMax", -7, s2.peekMax());
        check("t2 popMax", -7, s2.popMax());
        s2.push(3);
        check("t2 top after reuse", 3, s2.top());

        // Test 3: duplicate max values, popMax must remove the topmost one
        Solution s3 = new Solution();
        s3.push(5);
        s3.push(1);
        s3.push(5);
        s3.push(2);
        s3.push(5);
        check("t3 popMax first", 5, s3.popMax());
        check("t3 top after first popMax", 2, s3.top());
        check("t3 peekMax", 5, s3.peekMax());
        check("t3 popMax second", 5, s3.popMax());
        check("t3 top after second popMax", 2, s3.top());
        check("t3 pop", 2, s3.pop());
        check("t3 top", 5, s3.top());

        // Test 4: boundary values from the constraints
        Solution s4 = new Solution();
        s4.push(-10000000);
        s4.push(-5);
        s4.push(0);
        s4.push(10000000);
        check("t4 peekMax", 10000000, s4.peekMax());
        check("t4 top", 10000000, s4.top());
        check("t4 popMax", 10000000, s4.popMax());
        check("t4 peekMax after popMax", 0, s4.peekMax());
        check("t4 pop", 0, s4.pop());
        check("t4 top", -5, s4.top());

        // Test 5: max buried at the bottom
        Solution s5 = new Solution();
        s5.push(9);
        s5.push(2);
        s5.push(3);
        check("t5 popMax", 9, s5.popMax());
        check("t5 top", 3, s5.top());
        check("t5 peekMax", 3, s5.peekMax());
        check("t5 pop", 3, s5.pop());
        check("t5 pop again", 2, s5.pop());

        // Test 6: drain the stack completely, then reuse it
        Solution s6 = new Solution();
        s6.push(4);
        s6.push(4);
        check("t6 pop", 4, s6.pop());
        check("t6 popMax", 4, s6.popMax());
        s6.push(1);
        check("t6 peekMax after refill", 1, s6.peekMax());
        check("t6 top", 1, s6.top());

        // Test 7: repeated popMax over duplicates with a smaller value on top
        Solution s7 = new Solution();
        s7.push(2);
        s7.push(2);
        s7.push(2);
        s7.push(1);
        check("t7 popMax one", 2, s7.popMax());
        check("t7 top", 1, s7.top());
        check("t7 popMax two", 2, s7.popMax());
        check("t7 peekMax", 2, s7.peekMax());
        check("t7 pop", 1, s7.pop());
        check("t7 top", 2, s7.top());

        // Test 8: mixed pushes, verify full ordering after popMax
        Solution s8 = new Solution();
        s8.push(1);
        s8.push(5);
        s8.push(5);
        s8.push(3);
        s8.push(5);
        check("t8 popMax one", 5, s8.popMax());
        check("t8 top", 3, s8.top());
        check("t8 peekMax", 5, s8.peekMax());
        check("t8 popMax two", 5, s8.popMax());
        check("t8 pop", 3, s8.pop());
        check("t8 top", 5, s8.top());
        check("t8 popMax three", 5, s8.popMax());
        check("t8 final top", 1, s8.top());

        if (fails > 0) {
            System.out.println(fails + " check(s) failed");
            System.exit(1);
        } else {
            System.out.println("all checks passed");
        }
    }
}
