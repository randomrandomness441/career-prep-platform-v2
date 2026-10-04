import java.util.*;

class Solution {

    private List<Integer> stack;

    public Solution() {
        stack = new ArrayList<>();
    }

    public void push(int x) {
        stack.add(x);
    }

    public int pop() {
        return stack.remove(stack.size() - 1);
    }

    public int top() {
        return stack.get(stack.size() - 1);
    }

    public int peekMax() {
        int best = stack.get(0);
        for (int v : stack) {
            if (v > best) {
                best = v;
            }
        }
        return best;
    }

    public int popMax() {
        int best = peekMax();
        stack.remove(Integer.valueOf(best));
        return best;
    }
}
