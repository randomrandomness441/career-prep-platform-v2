#include <future>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>

// Runs f() on its own thread and hands back a future for its result.
template <typename F>
std::future<std::invoke_result_t<F>> run_task(F f) {
    using R = std::invoke_result_t<F>;
    auto p = std::make_shared<std::promise<R>>();
    std::future<R> fut = p->get_future();

    std::thread([p, f = std::move(f)]() mutable {
        // TODO: what happens to the caller waiting on fut if f() throws?
        if constexpr (std::is_void_v<R>) {
            f();
            p->set_value();
        } else {
            p->set_value(f());
        }
    }).detach();

    return fut;
}
