### How it's called

```cpp
DirNode root;
root.files = {"a.txt", "b.txt"};
auto sub = std::make_unique<DirNode>();
sub->files = {"c.txt"};
root.subdirs.push_back(std::move(sub));

int renamed = parallel_rename_all(root, [](const std::string& old){ return old + ".bak"; });
// renamed == 3
// root.files == {"a.txt.bak", "b.txt.bak"}; root.subdirs[0]->files == {"c.txt.bak"}
```

A single call, internally spawning threads per subdirectory branch and joining them
before returning the combined count.
