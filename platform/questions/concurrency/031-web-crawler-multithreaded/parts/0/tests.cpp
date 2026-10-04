// Harness for "Web Crawler Multithreaded". Includes the candidate's file
// verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

// A fixed link graph. "bad" throws instead of returning its links -- so "e",
// only reachable through "bad", must never appear in the result; everything
// else, reachable by other paths, must.
class GraphParser : public HtmlParser {
public:
    std::vector<std::string> getUrls(const std::string& url) override {
        if (url == "bad") throw std::runtime_error("simulated network error");
        auto it = graph().find(url);
        return it == graph().end() ? std::vector<std::string>{} : it->second;
    }

private:
    static const std::map<std::string, std::vector<std::string>>& graph() {
        static const std::map<std::string, std::vector<std::string>> g = {
            {"start", {"a", "b", "bad"}},
            {"a", {"c"}},
            {"b", {"d"}},
            {"c", {}},
            {"d", {}},
            // "bad" would link to "e", but getUrls("bad") throws instead.
        };
        return g;
    }
};

}  // namespace

int main() {
    // ── 1. a page that throws must not crash the process ─────────────────
    // Runs in a forked child: fork() before any threads exist, so only the
    // child can be killed by an escaping exception, and this process
    // survives to report the result either way.
    {
        pid_t pid = fork();
        if (pid < 0) { std::perror("fork"); return 1; }

        if (pid == 0) {
            GraphParser parser;
            WebCrawler crawler;
            std::vector<std::string> result = crawler.crawl("start", parser, 4);
            _exit(0);
        }

        int status = 0;
        waitpid(pid, &status, 0);
        if (WIFSIGNALED(status)) {
            std::printf(
                "crawl() let a page's exception escape a worker thread: the process "
                "was killed by signal %d (%s), consistent with std::terminate(). "
                "Wrap the getUrls() call in try/catch and still run the "
                "active_workers-- / notify_all() bookkeeping on the failure path.\n",
                WTERMSIG(status), strsignal(WTERMSIG(status)));
            return 1;
        }
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            std::printf("crawl() with a throwing page did not complete cleanly "
                        "(child exit status %d)\n", WIFEXITED(status) ? WEXITSTATUS(status) : -1);
            return 1;
        }
    }

    // ── 2. correctness: exactly the reachable set, nothing from "bad"'s
    //      unrecoverable links, across several worker-count and trial
    //      combinations ────────────────────────────────────────────────────
    const std::set<std::string> expected = {"start", "a", "b", "bad", "c", "d"};
    for (int trial = 0; trial < 8; ++trial) {
        GraphParser parser;
        WebCrawler crawler;
        SHAKE();
        std::vector<std::string> result = crawler.crawl("start", parser, 4);
        std::set<std::string> got(result.begin(), result.end());

        if (got != expected) {
            std::printf("trial %d: crawl() returned a different set than expected\n", trial);
            for (const auto& u : got) if (!expected.count(u))
                std::printf("  unexpected: %s\n", u.c_str());
            for (const auto& u : expected) if (!got.count(u))
                std::printf("  missing: %s\n", u.c_str());
            return 1;
        }
        if (result.size() != got.size()) {
            std::printf("trial %d: crawl() returned %zu URLs but only %zu distinct -- "
                        "a URL was visited more than once\n", trial, result.size(), got.size());
            return 1;
        }
    }

    // ── 3. no throwing page at all: plain correctness, more workers than
    //      there are ever URLs in flight, to stress worker shutdown ───────
    {
        class NoFailParser : public HtmlParser {
        public:
            std::vector<std::string> getUrls(const std::string& url) override {
                if (url == "start") return {"x", "y", "z"};
                return {};
            }
        };
        NoFailParser parser;
        WebCrawler crawler;
        std::vector<std::string> result = crawler.crawl("start", parser, 16);
        std::set<std::string> got(result.begin(), result.end());
        std::set<std::string> want = {"start", "x", "y", "z"};
        if (got != want) {
            std::printf("no-failure case: got a different set than {start,x,y,z}\n");
            return 1;
        }
    }

    std::printf("a throwing page doesn't crash or hang the pool, and 8 trials of the "
                "reachable-set check plus a clean no-failure run all matched exactly\n");
    return 0;
}
