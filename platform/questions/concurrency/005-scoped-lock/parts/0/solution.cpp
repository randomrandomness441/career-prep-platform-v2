#include <mutex>

class Account {
public:
    explicit Account(long initial) : balance_(initial) {}

    Account(const Account&) = delete;
    Account& operator=(const Account&) = delete;

    long balance() const {
        std::lock_guard<std::mutex> g(m_);
        return balance_;
    }

private:
    mutable std::mutex m_;
    long balance_;

    friend bool transfer(Account& from, Account& to, long amount);
};

bool transfer(Account& from, Account& to, long amount) {
    // Same account: nothing to move, and locking one std::mutex twice on the
    // same thread is undefined behaviour (in practice, an instant deadlock).
    // std::scoped_lock and std::lock both have a precondition that the mutexes
    // are distinct, so this check is not optional.
    if (&from == &to) return true;

    // One statement locks both. std::scoped_lock uses a try-and-back-off
    // algorithm: it blocks on one mutex, then try_locks the rest; if any
    // try_lock fails it releases everything it holds and restarts, blocking on
    // the one that refused. It never sits holding lock A while waiting for
    // lock B, so the cycle that causes deadlock cannot form — whatever order
    // the callers pass their accounts in.
    std::scoped_lock lk(from.m_, to.m_);

    // Check and update inside one critical section: the balance cannot change
    // between the test and the subtraction, so no account can go negative.
    if (from.balance_ < amount) return false;
    from.balance_ -= amount;
    to.balance_ += amount;
    return true;
}
