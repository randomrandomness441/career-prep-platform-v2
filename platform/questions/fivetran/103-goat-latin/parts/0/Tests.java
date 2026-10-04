public class Tests {
    static int fails = 0;

    public static void main(String[] args) {
        check("I speak Goat Latin", "Imaa peaksmaaa oatGmaaaa atinLmaaaaa");
        check("The quick brown fox jumped over the lazy dog",
              "heTmaa uickqmaaa rownbmaaaa oxfmaaaaa umpedjmaaaaaa overmaaaaaaa hetmaaaaaaaa azylmaaaaaaaaa ogdmaaaaaaaaaa");
        check("goat", "oatgmaa");
        check("a", "amaa");
        check("b", "bmaa");
        check("Each day I code", "Eachmaa aydmaaa Imaaaa odecmaaaaa");
        check("Uber eats apples", "Ubermaa eatsmaaa applesmaaaa");
        check("yellow oak", "ellowymaa oakmaaa");
        check("hi there", "ihmaa heretmaa");

        if (fails > 0) {
            System.out.println(fails + " test(s) failed");
            System.exit(1);
        } else {
            System.out.println("all checks passed");
        }
    }

    static void check(String input, String expected) {
        String actual = new Solution().toGoatLatin(input);
        if (!expected.equals(actual)) {
            fails++;
            System.out.println("FAILED: input=\"" + input + "\"");
            System.out.println("  expected: \"" + expected + "\"");
            System.out.println("  actual:   \"" + actual + "\"");
        }
    }
}