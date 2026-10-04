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

// TODO: lock both accounts and move `amount` from `from` to `to`, returning
//       false (no state changed) if `from` doesn't have enough. Two threads
//       can call this on the same pair of accounts in opposite directions at
//       the same time -- and a caller can pass the same account as both
//       `from` and `to`.
bool transfer(Account& from, Account& to, long amount) {
    return false;
}
