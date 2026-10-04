// Harness for "Parallel Directory Rename". Includes the candidate's file
// verbatim.
#include "solution.hpp"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {

// Builds a tree of branching factor `branch`, `depth` levels deep, with
// `files_per_node` files in every directory (including the root).
std::unique_ptr<DirNode> make_tree(int branch, int depth, int files_per_node, int& next_id) {
    auto node = std::make_unique<DirNode>();
    for (int f = 0; f < files_per_node; ++f) {
        node->files.push_back("file" + std::to_string(next_id++) + ".txt");
    }
    if (depth > 0) {
        for (int b = 0; b < branch; ++b) {
            node->subdirs.push_back(make_tree(branch, depth - 1, files_per_node, next_id));
        }
    }
    return node;
}

int total_files(const DirNode& node) {
    int n = static_cast<int>(node.files.size());
    for (auto& s : node.subdirs) n += total_files(*s);
    return n;
}

bool all_renamed(const DirNode& node) {
    for (auto& f : node.files) {
        if (f.size() < 8 || f.compare(f.size() - 8, 8, "_renamed") != 0) return false;
    }
    for (auto& s : node.subdirs) {
        if (!all_renamed(*s)) return false;
    }
    return true;
}

}  // namespace

int main() {
    int next_id = 0;
    auto tree = make_tree(/*branch=*/4, /*depth=*/4, /*files_per_node=*/5, next_id);
    int expected = total_files(*tree);

    auto rename_fn = [](const std::string& name) { return name + "_renamed"; };
    int renamed = parallel_rename_all(*tree, rename_fn);

    if (renamed != expected) {
        std::printf("parallel_rename_all() returned %d, expected %d -- the count is wrong "
                    "under concurrent renaming (file names ARE renamed correctly, but the "
                    "shared counter lost updates)\n", renamed, expected);
        return 1;
    }
    if (!all_renamed(*tree)) {
        std::printf("some file in the tree was not actually renamed\n");
        return 1;
    }

    std::printf("a %d-file tree (branching 4, depth 4): every file renamed, count exact "
                "(%d)\n", expected, renamed);
    return 0;
}
