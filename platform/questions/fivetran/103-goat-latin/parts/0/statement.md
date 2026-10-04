Standard LeetCode problem, tagged to Fivetran's interview loop -- solve it in Java, no different from solving it on leetcode.com.

You are given a string `sentence` whose words are separated by single spaces. Convert every word to Goat Latin and return the resulting sentence. If a word begins with a vowel, append `"ma"` to the end of the word. If it begins with a consonant, remove the first letter, append that letter to the end of the word, and then append `"ma"`. Finally, add one `'a'` per word index. The first word gets one `'a'`, the second gets two, and so on. Vowels are `a`, `e`, `i`, `o`, `u` and the check is case-insensitive. The letter `y` counts as a consonant.

`public String toGoatLatin(String sentence)`

Example 1:
Input: sentence = "I speak Goat Latin"
Output: "Imaa peaksmaaa oatGmaaaa atinLmaaaaa"

Example 2:
Input: sentence = "The quick brown fox jumped over the lazy dog"
Output: "heTmaa uickqmaaa rownbmaaaa oxfmaaaaa umpedjmaaaaaa overmaaaaaaa hetmaaaaaaaa azylmaaaaaaaaa ogdmaaaaaaaaaa"

Example 3:
Input: sentence = "Each day"
Output: "Eachmaa aydmaaa"

Constraints:
- 1 <= sentence.length <= 150
- sentence consists of English letters and spaces.
- sentence does not contain any leading or trailing spaces.
- All the words in sentence are separated by a single space.