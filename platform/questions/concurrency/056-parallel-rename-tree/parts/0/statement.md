# Parallel Directory Rename

## ELI5: renaming every nametag in a company org chart, department by department

Imagine a company org chart: departments contain smaller teams, teams contain people
with nametags. You need to rename every single nametag, company-wide, using some
renaming rule. Instead of walking the whole chart alone, one nametag at a time, you hand
each top-level department off to its own crew, and each of *those* crews hands their own
sub-teams off further, everyone renaming their own branch of the chart at the same time.
At the end, you need one correct grand total of how many nametags got changed, without
different crews' counts trampling on each other.

## What you're actually building

Rename every file in a directory tree, processing subdirectories in parallel.

```cpp
struct DirNode {
    std::vector<std::unique_ptr<DirNode>> subdirs;
    std::vector<std::string> files;
};

int parallel_rename_all(DirNode& node,
const std::function<std::string(const std::string&)>& rename_fn);
```

`DirNode` is an in-memory stand-in for a real directory (this exercise doesn't touch an
actual filesystem, to keep it deterministic and side-effect-free). `parallel_rename_all`
replaces every filename in `node` and every descendant, in place, with
`rename_fn(old_name)`, and returns the total number of files renamed.

## Requirements

1. Every file in the tree, at every depth, is renamed exactly once.
2. **Subdirectories are processed in parallel** (not one at a time), separate crews,
 working simultaneously on separate branches.
3. The returned count is exactly right, under real concurrent execution across a tree
 with many subdirectories at multiple levels.

## Why the constraints exist

**No global/shared mutable counter**, if you need a running total across concurrently
running subtrees, get it from each recursive call's own return value instead. Each crew
reports its own count back up the chart; nobody's writing to one shared tally sheet at
the same time as everyone else.
