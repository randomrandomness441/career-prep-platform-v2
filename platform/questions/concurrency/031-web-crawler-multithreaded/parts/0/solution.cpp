#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

class HtmlParser {
public:
    virtual ~HtmlParser() = default;
    virtual std::vector<std::string> getUrls(const std::string& url) = 0;
};

// Same pool-of-workers shape as the naive version. The one addition:
// getUrls() runs inside a try/catch, and the bookkeeping that tells the pool
// "this worker is done with its URL" happens whether that call succeeded or
// threw -- so one bad page can't take down the process (an exception
// escaping a std::thread's entry function calls std::terminate()) or hang
// every other worker (skipping active_workers-- and notify_all() would
// leave the rest of the pool waiting on a state change that never comes).
class WebCrawler {
public:
    std::vector<std::string> crawl(const std::string& startUrl, HtmlParser& parser,
                                    int num_workers = 8) {
        std::mutex m;
        std::condition_variable cv;
        std::queue<std::string> q;
        std::unordered_set<std::string> visited;
        int active_workers = 0;
        bool done = false;

        q.push(startUrl);
        visited.insert(startUrl);

        auto worker = [&] {
            while (true) {
                std::unique_lock<std::mutex> lk(m);
                cv.wait(lk, [&] { return !q.empty() || done; });
                if (done && q.empty()) return;
                std::string url = std::move(q.front());
                q.pop();
                ++active_workers;
                lk.unlock();

                std::vector<std::string> urls;
                bool ok = true;
                try {
                    urls = parser.getUrls(url);
                } catch (...) {
                    // This page's own links are unrecoverable, but that
                    // doesn't excuse the pool from finishing correctly --
                    // every other URL already queued still gets crawled.
                    ok = false;
                }

                lk.lock();
                if (ok) {
                    for (auto& u : urls) {
                        if (visited.insert(u).second) q.push(u);
                    }
                }
                --active_workers;
                if (q.empty() && active_workers == 0) done = true;
                cv.notify_all();
            }
        };

        std::vector<std::thread> threads;
        for (int i = 0; i < num_workers; ++i) threads.emplace_back(worker);
        for (auto& t : threads) t.join();

        return std::vector<std::string>(visited.begin(), visited.end());
    }
};
