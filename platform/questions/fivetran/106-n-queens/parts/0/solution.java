import java.util.*;

class Solution {
    public List<List<String>> solveNQueens(int n) {
        List<List<String>> result = new ArrayList<>();
        char[][] board = new char[n][n];
        for (char[] row : board) {
            Arrays.fill(row, '.');
        }
        backtrack(board, 0, result);
        return result;
    }

    private void backtrack(char[][] board, int row, List<List<String>> result) {
        if (row == board.length) {
            List<String> snapshot = new ArrayList<>();
            for (char[] r : board) {
                snapshot.add(new String(r));
            }
            result.add(snapshot);
            return;
        }
        for (int col = 0; col < board.length; col++) {
            if (isValid(board, row, col)) {
                board[row][col] = 'Q';
                backtrack(board, row + 1, result);
                board[row][col] = '.';
            }
        }
    }

    private boolean isValid(char[][] board, int row, int col) {
        for (int i = 0; i < row; i++) {
            if (board[i][col] == 'Q') {
                return false;
            }
            if (col - (row - i) >= 0 && board[i][col - (row - i)] == 'Q') {
                return false;
            }
            if (col + (row - i) < board.length && board[i][col + (row - i)] == 'Q') {
                return false;
            }
        }
        return true;
    }
}