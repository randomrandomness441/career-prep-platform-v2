// Harness for remove_elements. Includes the candidate's file verbatim.
#include "solution.hpp"
#include <cstdio>
#include <vector>

namespace {
ListNode* build(const std::vector<int>& xs) {
    ListNode dummy(0);
    ListNode* tail = &dummy;
    for (int x : xs) {
        tail->next = new ListNode(x);
        tail = tail->next;
    }
    return dummy.next;
}
std::vector<int> to_vec(ListNode* head) {
    std::vector<int> out;
    for (ListNode* cur = head; cur; cur = cur->next) out.push_back(cur->val);
    return out;
}
void free_list(ListNode* head) {
    while (head) { ListNode* nxt = head->next; delete head; head = nxt; }
}
}  // namespace

static int fails = 0;
static void check(ListNode* got, const std::vector<int>& want, const char* name) {
    std::vector<int> got_vec = to_vec(got);
    if (got_vec != want) {
        std::printf("%s: got size %zu, expected size %zu\n", name, got_vec.size(), want.size());
        ++fails;
    }
    free_list(got);
}

int main() {
    check(remove_elements(build({1,2,6,3,4,5,6}), 6), {1,2,3,4,5}, "interior and trailing matches");
    check(remove_elements(build({}), 1), {}, "empty list");
    check(remove_elements(build({7,7,7,7}), 7), {}, "entire list matches (all leading)");
    check(remove_elements(build({7,1,7,7,2,7}), 7), {1,2}, "leading run plus scattered matches");
    check(remove_elements(build({1,2,3}), 5), {1,2,3}, "no matches at all");
    check(remove_elements(build({1}), 1), {}, "single-node list, matches");

    if (fails) { std::printf("%d check(s) failed\n", fails); return 1; }
    std::printf("all linked-list removal cases correct\n");
    return 0;
}
