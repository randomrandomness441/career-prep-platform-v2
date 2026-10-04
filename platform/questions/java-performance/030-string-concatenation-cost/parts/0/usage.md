Plain text, three numbered points. Example shape:

```
1. Each s = s + ... copies the entire current string plus the new piece.
   As s grows, each concatenation costs more, and this happens every
   iteration -- the total across n iterations sums to roughly n^2, not n.
2. The compiler sees the whole concatenation expression at once and
   generates one efficient combining operation for all the pieces together,
   rather than a sequence of rebuild-the-whole-string-so-far steps. There's
   no growing intermediate result being repeatedly rebuilt.
3. Yes, same cost. String hasn't shared backing storage between a string and
   its substrings since Java 7u6 -- every substring() call copies a new
   array, so repeated shrinking pays the same near-full-copy cost as
   repeated growing. StringBuilder still helps since setLength()/
   deleteCharAt() shrink in place without a full reallocation.
```
