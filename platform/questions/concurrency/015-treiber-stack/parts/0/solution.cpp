#include <atomic>
#include <utility>

// A Treiber stack: a singly-linked list whose head pointer is an atomic, and
// where both operations are a compare-and-swap retry loop on that one pointer.
//
// The hard part is not push and pop. It is knowing when a popped node is safe
// to delete. This implementation does NOT solve that problem — it moves popped
// nodes to a retired list and frees them only in the destructor, which is
// single-threaded. See "Where this solution fails" in the reading.
template <typename T>
class LockFreeStack {
public:
    LockFreeStack() = default;

    LockFreeStack(const LockFreeStack&) = delete;
    LockFreeStack& operator=(const LockFreeStack&) = delete;

    // Destruction is single-threaded by contract: nobody may be pushing or
    // popping here. Acquire loads pair with the release stores in push() and
    // retire(), so this thread sees every field every other thread wrote.
    ~LockFreeStack() {
        Node* n = head_.load(std::memory_order_acquire);
        while (n) { Node* nx = n->next; delete n; n = nx; }
        n = retired_.load(std::memory_order_acquire);
        while (n) { Node* nx = n->retire_next; delete n; n = nx; }
    }

    void push(T value) {
        Node* n = new Node(std::move(value));

        // n is private to this thread until the CAS below succeeds, so writing
        // n->next needs no synchronisation at all.
        n->next = head_.load(std::memory_order_relaxed);

        // On failure compare_exchange_weak writes the *current* head back into
        // its first argument — which is n->next — so the loop body is empty:
        // the retry is already set up for us.
        //
        // _weak, not _strong: on arm64 a CAS is a load-exclusive /
        // store-exclusive pair that can fail spuriously. _strong has to hide
        // that behind an extra inner loop. Inside a retry loop we do not care
        // why we failed, so we take the cheaper one.
        //
        // release on success: everything written to the node (its value, its
        // next pointer) must be visible to whichever thread later acquires the
        // head and follows this pointer. Without it a popper can see the
        // pointer and stale bytes behind it.
        while (!head_.compare_exchange_weak(n->next, n,
                                           std::memory_order_release,
                                           std::memory_order_relaxed)) {
        }
    }

    // Returns false if the stack was empty at the moment we looked.
    bool pop(T& out) {
        // acquire: pairs with push's release, so if we see this node we also
        // see its contents.
        Node* old = head_.load(std::memory_order_acquire);

        // Note the short-circuit. If old is null we must not evaluate
        // old->next, and the CAS is not attempted at all.
        while (old && !head_.compare_exchange_weak(old, old->next,
                                                  std::memory_order_acquire,
                                                  std::memory_order_acquire)) {
        }
        if (!old) return false;

        // Safe only because nodes are never freed while the stack is live.
        // If pop() ended in `delete old`, another thread sitting between its
        // own head_.load() and its own `old->next` would be reading freed
        // memory.
        out = std::move(old->data);
        retire(old);
        return true;
    }

    bool empty() const { return head_.load(std::memory_order_acquire) == nullptr; }

private:
    struct Node {
        explicit Node(T v) : data(std::move(v)) {}
        T data;
        Node* next = nullptr;
        // A SEPARATE link for the retired list. Reusing `next` here would be a
        // data race: a thread whose CAS is still in flight may be reading
        // old->next for a node we have already popped.
        Node* retire_next = nullptr;
    };

    // Same CAS loop as push, on a second list. Nothing ever reads these links
    // until the destructor, so the only requirement is that the destructor's
    // acquire load sees them — hence release here.
    void retire(Node* n) {
        n->retire_next = retired_.load(std::memory_order_relaxed);
        while (!retired_.compare_exchange_weak(n->retire_next, n,
                                               std::memory_order_release,
                                               std::memory_order_relaxed)) {
        }
    }

    std::atomic<Node*> head_{nullptr};
    std::atomic<Node*> retired_{nullptr};
};
