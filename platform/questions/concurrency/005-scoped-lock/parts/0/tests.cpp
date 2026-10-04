// Harness for "Two Mutexes Without a Deadlock".
#include "solution.hpp"

#include "concur/stress.hpp"
#include <cstdio>
#include <memory>
#include <random>
#include <thread>
#include <vector>

namespace {
const int  kThreads = 8;
const long kIters   = 40000;
}  // namespace

int main() {
    // ── 1. single-threaded behaviour ─────────────────────────────────────
    {
        Account a(100), b(50);
        if (!transfer(a, b, 30)) { std::printf("a legal transfer was refused\n"); return 1; }
        if (a.balance() != 70 || b.balance() != 80) {
            std::printf("after moving 30 from a(100) to b(50): a=%ld b=%ld, expected 70 and 80\n",
                        a.balance(), b.balance());
            return 1;
        }
        if (transfer(a, b, 1000)) {
            std::printf("an overdraft was allowed\n");
            return 1;
        }
        if (a.balance() != 70 || b.balance() != 80) {
            std::printf("a refused transfer changed the balances: a=%ld b=%ld\n",
                        a.balance(), b.balance());
            return 1;
        }
    }

    // ── 2. opposite-direction transfers must not deadlock ────────────────
    // Half the threads move money A->B, half move it B->A, as fast as they can.
    // If transfer() locks "from" then "to", one thread ends up holding A and
    // waiting for B while another holds B and waiting for A, and neither ever
    // moves again. This test hangs rather than failing, which is exactly what a
    // deadlock does in production.
    {
        Account a(1000000), b(1000000);
        const long total_before = a.balance() + b.balance();

        std::vector<std::thread> ts;
        for (int t = 0; t < kThreads; ++t) {
            ts.emplace_back([&, t] {
                SHAKE();
                for (long i = 0; i < kIters; ++i) {
                    if (t % 2 == 0) transfer(a, b, 1);
                    else            transfer(b, a, 1);
                }
                SHAKE();
            });
        }
        for (std::thread& th : ts) th.join();

        const long total_after = a.balance() + b.balance();
        if (total_after != total_before) {
            std::printf("money was created or destroyed: %ld before, %ld after "
                        "(a=%ld b=%ld)\n", total_before, total_after, a.balance(), b.balance());
            return 1;
        }
        if (a.balance() < 0 || b.balance() < 0) {
            std::printf("a balance went negative: a=%ld b=%ld — the check and the "
                        "subtraction were not in the same critical section\n",
                        a.balance(), b.balance());
            return 1;
        }
    }

    // ── 3. transferring to the same account ──────────────────────────────
    // Locking one std::mutex twice on the same thread is undefined behaviour.
    // In practice it is a deadlock, and this call has to survive it.
    {
        Account a(500);
        transfer(a, a, 100);
        if (a.balance() != 500) {
            std::printf("self-transfer changed the balance: %ld, expected 500\n", a.balance());
            return 1;
        }
    }

    // ── 4. many accounts, random pairs, random directions ────────────────
    // With six accounts there are many possible lock orders, so a solution that
    // only handles the two-account case by ordering its arguments a special way
    // is caught here.
    {
        const int kAccounts = 6;
        std::vector<std::unique_ptr<Account>> acct;
        for (int i = 0; i < kAccounts; ++i) acct.push_back(std::make_unique<Account>(10000));
        long total_before = 0;
        for (const std::unique_ptr<Account>& p : acct) total_before += p->balance();

        std::vector<std::thread> ts;
        for (int t = 0; t < kThreads; ++t) {
            ts.emplace_back([&, t] {
                std::mt19937 rng(static_cast<unsigned>(t) * 7919u + 13u);
                SHAKE();
                for (long i = 0; i < kIters / 2; ++i) {
                    int x = static_cast<int>(rng() % kAccounts);
                    int y = static_cast<int>(rng() % kAccounts);
                    transfer(*acct[x], *acct[y], static_cast<long>(rng() % 5));
                }
                SHAKE();
            });
        }
        for (std::thread& th : ts) th.join();

        long total_after = 0;
        for (const std::unique_ptr<Account>& p : acct) {
            if (p->balance() < 0) {
                std::printf("an account went negative: %ld\n", p->balance());
                return 1;
            }
            total_after += p->balance();
        }
        if (total_after != total_before) {
            std::printf("six accounts: %ld before, %ld after — money leaked\n",
                        total_before, total_after);
            return 1;
        }
    }

    std::printf("%d threads, %ld transfers each in both directions: no deadlock, "
                "no money created or lost, no negative balances\n", kThreads, kIters);
    return 0;
}
