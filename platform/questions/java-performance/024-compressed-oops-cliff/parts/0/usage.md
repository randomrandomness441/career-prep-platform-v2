Plain text, three numbered points. Example shape:

```
1. The JVM needs room for more than just objects at the boundary --
   metadata, alignment padding, non-object regions all need to fit inside
   the addressable range too. Using the entire theoretical maximum for
   objects alone would leave no margin for anything else, so the real
   ergonomic cutoff sits a bit below the naive 32GB number.
2. Crossing from 30GB to 40GB crosses the compressed-oops cutoff entirely --
   every object in the heap gets roughly a third bigger. The "safer" bigger
   heap can hold less effective data and generate more GC work per unit of
   real data than staying just under the cutoff (say 31GB) would have.
3. Only reference fields double in size when compression is lost --
   primitives are unaffected, they were never compressed pointers. An
   object with no reference fields would see almost no size increase; one
   with many reference fields would see a bigger percentage jump than the
   measured 36%, since more of its total size is made of things that just
   doubled.
```
