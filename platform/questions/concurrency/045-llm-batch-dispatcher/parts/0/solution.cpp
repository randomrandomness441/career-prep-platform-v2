#include <functional>
#include <future>
#include <mutex>
#include <utility>
#include <vector>

// A request-batching dispatcher: the shape behind most real LLM inference
// servers. GPUs are far more efficient processing many prompts in one
// forward pass than one at a time, so many independent callers' requests
// get collected into a batch and sent through the (expensive, shared)
// model together -- each caller still gets back exactly their own
// response, via a future, with no idea the batching even happened.
struct Request { int value; };
struct Response { int value; };

class BatchDispatcher {
public:
    explicit BatchDispatcher(
        std::function<std::vector<Response>(const std::vector<Request>&)> process_batch)
        : process_batch_(std::move(process_batch)) {}

    // Called concurrently from many client threads. Queues the request and
    // returns a future for its eventual response.
    std::future<Response> submit(Request req) {
        std::promise<Response> promise;
        std::future<Response> fut = promise.get_future();
        std::lock_guard<std::mutex> lk(m_);
        pending_.emplace_back(std::move(req), std::move(promise));
        return fut;
    }

    // Drains everything currently pending, runs the batch, and fulfils
    // every caller's promise with their own response. In a real server a
    // dedicated thread calls this on a timer or once max_batch_size is
    // reached; this class doesn't dictate the policy, just the mechanics.
    void flush() {
        std::vector<std::pair<Request, std::promise<Response>>> batch;
        {
            std::lock_guard<std::mutex> lk(m_);
            batch = std::move(pending_);
            pending_.clear();
        }
        if (batch.empty()) return;

        std::vector<Request> reqs;
        reqs.reserve(batch.size());
        for (auto& item : batch) reqs.push_back(item.first);

        std::vector<Response> results = process_batch_(reqs);

        for (std::size_t i = 0; i < batch.size(); ++i) {
            batch[i].second.set_value(results[i]);
        }
    }

    std::size_t pending_count() const {
        std::lock_guard<std::mutex> lk(m_);
        return pending_.size();
    }

private:
    std::function<std::vector<Response>(const std::vector<Request>&)> process_batch_;
    mutable std::mutex m_;
    std::vector<std::pair<Request, std::promise<Response>>> pending_;
};
