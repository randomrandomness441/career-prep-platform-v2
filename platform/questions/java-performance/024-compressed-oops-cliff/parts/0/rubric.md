A good answer covers:

- **Why the real cutoff sits below the theoretical maximum.** The JVM needs the heap
  layout (metadata, alignment padding, non-object regions) to fit within the addressable
  range a 32-bit compressed reference can reach, not just the objects themselves — using
  the entire theoretical maximum for objects alone, with zero margin for anything else,
  would risk being unable to address the full heap safely. A good answer names that real
  systems need a margin below a hard theoretical limit, not that the limit itself is
  wrong.
- **The real risk of over-provisioning past the boundary.** Bumping from 30GB to 40GB
  crosses the compressed-oops cutoff entirely, making every object in the heap roughly a
  third bigger — the "safer, more headroom" heap can end up holding meaningfully less
  *effective* data and generating more garbage collection work per unit of real data
  than a heap sized to land just under the cutoff (say, 31GB) would. A good answer
  names this as a real, counterintuitive risk specific to crossing this exact boundary,
  not a generic "bigger heaps are always worse" claim.
- **What actually grows, and why the percentage would differ.** Only reference fields
  (object pointers) go from 4 to 8 bytes when compression is lost — primitive fields
  (`int`, `long`, `double`, etc.) are unaffected, since they were never compressed
  pointers to begin with. An object with zero reference fields would see close to no
  size increase from crossing this boundary (object header overhead aside); an object
  with many reference fields would see a *larger* percentage increase than the
  measured 36%, since a bigger fraction of its total size is made of things that just
  doubled. A good answer states the mechanism (only pointers are affected) before
  predicting the direction of the change.

NEEDS_WORK if the answer thinks the 32GB folklore number is exact, recommends
over-provisioning heap size without mentioning this boundary, or can't explain why the
percentage effect depends on how reference-heavy an object's fields are.

## Configuration

**Risky — "more headroom" crosses the boundary without anyone noticing:**
```
-Xmx40g    # every object in the heap just got ~36% bigger
```

**Correct — land just under the real cutoff instead:**
```
-Xmx31g    # keeps UseCompressedOops on; verify with:
java -Xmx31g -XX:+PrintFlagsFinal -version | grep UseCompressedOops
```

**Alternative — if you genuinely need more than ~32GB, extend the addressable range instead of accepting the loss:**
```
-Xmx48g -XX:ObjectAlignmentInBytes=16    # trades more per-object padding for compression past 32GB
```

