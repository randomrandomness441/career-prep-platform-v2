#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// A directory tree, in memory (a stand-in for a real filesystem, so this
// exercise is deterministic and touches no actual disk).
struct DirNode {
    std::vector<std::unique_ptr<DirNode>> subdirs;
    std::vector<std::string> files;
};

// Renames every file in the tree (replacing each filename with
// rename_fn(old_name)), processing subdirectories in parallel -- one
// thread per subdirectory at this level, recursing further inside each.
// Returns the total number of files renamed.
//
// No shared counter anywhere: each recursive call returns its OWN count,
// and a parent only ever writes to its OWN, distinct slot in sub_counts --
// concurrent threads never touch the same memory, so there's nothing to
// synchronize at all.
int parallel_rename_all(DirNode& node, const std::function<std::string(const std::string&)>& rename_fn) {
    std::vector<std::thread> threads;
    std::vector<int> sub_counts(node.subdirs.size(), 0);

    for (std::size_t i = 0; i < node.subdirs.size(); ++i) {
        DirNode* sub = node.subdirs[i].get();
        threads.emplace_back([sub, &rename_fn, &sub_counts, i] {
            sub_counts[i] = parallel_rename_all(*sub, rename_fn);
        });
    }

    int count = 0;
    for (auto& f : node.files) {
        f = rename_fn(f);
        ++count;
    }

    for (auto& t : threads) t.join();
    for (int c : sub_counts) count += c;
    return count;
}
