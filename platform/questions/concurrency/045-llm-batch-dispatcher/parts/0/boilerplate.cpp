#include <functional>
#include <future>
#include <utility>
#include <vector>

// A request-batching dispatcher: many client threads call submit()
// concurrently, each getting back a future for their own eventual
// response. flush() drains everything queued so far, runs it through
// process_batch as one batch, and fulfils every caller's promise.
struct Request { int value; };
struct Response { int value; };

class BatchDispatcher {
public:
    explicit BatchDispatcher(
        std::function<std::vector<Response>(const std::vector<Request>&)> process_batch)
        : process_batch_(std::move(process_batch)) {}

    std::future<Response> submit(Request req) {
        // TODO: implement
        (void)req;
        std::promise<Response> promise;
        return promise.get_future();
    }

    void flush() {
        // TODO: implement
    }

    std::size_t pending_count() const {
        // TODO: implement
        return 0;
    }

private:
    std::function<std::vector<Response>(const std::vector<Request>&)> process_batch_;
    std::vector<std::pair<Request, std::promise<Response>>> pending_;
};
