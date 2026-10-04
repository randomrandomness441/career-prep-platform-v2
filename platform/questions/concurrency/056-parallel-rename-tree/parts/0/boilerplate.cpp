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
// rename_fn(old_name)), processing subdirectories in parallel. Returns the
// total number of files renamed.
int parallel_rename_all(DirNode& node, const std::function<std::string(const std::string&)>& rename_fn) {
    // TODO: implement
    (void)node;
    (void)rename_fn;
    return 0;
}
