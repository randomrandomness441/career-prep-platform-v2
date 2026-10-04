import java.util.*;

public class Tests {

    static int fails = 0;

    public static void main(String[] args) {
        check(
            new String[]{"alice,20,800,mtv", "alice,50,100,beijing"},
            new String[]{"alice,20,800,mtv", "alice,50,100,beijing"}
        );
        check(
            new String[]{"alice,20,800,mtv", "alice,50,1200,mtv"},
            new String[]{"alice,50,1200,mtv"}
        );
        check(
            new String[]{"alice,20,800,mtv", "bob,50,1200,mtv"},
            new String[]{"bob,50,1200,mtv"}
        );
        check(
            new String[]{"gary,30,1000,paris"},
            new String[]{}
        );
        check(
            new String[]{"gary,30,1001,paris"},
            new String[]{"gary,30,1001,paris"}
        );
        check(
            new String[]{"carl,0,10,nyc", "carl,60,20,la"},
            new String[]{"carl,0,10,nyc", "carl,60,20,la"}
        );
        check(
            new String[]{"carl,0,10,nyc", "carl,61,20,la"},
            new String[]{}
        );
        check(
            new String[]{"dave,0,100,a", "dave,50,100,b", "dave,100,100,a"},
            new String[]{"dave,0,100,a", "dave,50,100,b", "dave,100,100,a"}
        );
        check(
            new String[]{"erin,5,50,x", "erin,5,50,x"},
            new String[]{}
        );

        if (fails > 0) {
            System.out.println(fails + " check(s) failed");
            System.exit(1);
        } else {
            System.out.println("all checks passed");
        }
    }

    static void check(String[] transactions, String[] expected) {
        List<String> actual = Solution.invalidTransactions(transactions);
        List<String> actualSorted = new ArrayList<>(actual);
        List<String> expectedSorted = new ArrayList<>(Arrays.asList(expected));
        Collections.sort(actualSorted);
        Collections.sort(expectedSorted);
        if (!actualSorted.equals(expectedSorted)) {
            fails++;
            System.out.println("FAIL on input: " + Arrays.toString(transactions));
            System.out.println("  expected (sorted): " + expectedSorted);
            System.out.println("  actual (sorted):   " + actualSorted);
        }
    }
}