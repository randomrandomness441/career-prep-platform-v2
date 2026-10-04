import java.util.*;

class Solution {

    private Deque<Integer> mainStack;
    private Deque<Integer> maxStack;

    public Solution() {
        mainStack = new ArrayDeque<>();
        maxStack = new ArrayDeque<>();
    }

    public void push(int x) {
        mainStack.push(x);
        if (maxStack.isEmpty() || x >= maxStack.peek()) {
            maxStack.push(x);
        } else {
            maxStack.push(maxStack.peek());
        }
    }

    public int pop() {
        maxStack.pop();
        return mainStack.pop();
    }

    public int top() {
        return mainStack.peek();
    }

    public int peekMax() {
        return maxStack.peek();
    }

    public int popMax() {
        int m = maxStack.peek();
        Deque<Integer> buffer = new ArrayDeque<>();
        while (mainStack.peek() != m) {
            maxStack.pop();
            buffer.push(mainStack.pop());
        }
        mainStack.pop();
        maxStack.pop();
        while (!buffer.isEmpty()) {
            push(buffer.pop());
        }
        return m;
    }
}
