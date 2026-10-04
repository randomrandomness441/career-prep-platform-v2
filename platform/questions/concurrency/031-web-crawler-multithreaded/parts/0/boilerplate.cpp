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

// A pool of workers pulling URLs off a shared queue, tracking visited URLs
// and an active_workers count so the pool knows when there is truly nothing
// left to do (an empty queue isn't enough by itself: a worker mid-fetch
// might still be about to add more URLs to it).
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

                // TODO: getUrls() can throw (a network error, a malformed
                //       page). What happens to this worker, and to the rest
                //       of the pool, if it does and nothing here accounts
                //       for it?
                std::vector<std::string> urls = parser.getUrls(url);

                lk.lock();
                for (auto& u : urls) {
                    if (visited.insert(u).second) q.push(u);
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
