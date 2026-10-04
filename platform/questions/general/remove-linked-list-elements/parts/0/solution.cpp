struct ListNode {
    int val;
    ListNode* next;
    explicit ListNode(int v) : val(v), next(nullptr) {}
};

ListNode* remove_elements(ListNode* head, int val) {
    ListNode dummy(0);
    dummy.next = head;
    ListNode* cur = &dummy;
    while (cur->next) {
        if (cur->next->val == val) {
            ListNode* doomed = cur->next;
            cur->next = doomed->next;
            delete doomed;
        } else {
            cur = cur->next;
        }
    }
    return dummy.next;
}
