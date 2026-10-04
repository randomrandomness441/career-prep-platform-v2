#include <future>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>

template <typename F>
std::future<std::invoke_result_t<F>> run_task(F f) {
    using R = std::invoke_result_t<F>;
    auto p = std::make_shared<std::promise<R>>();
    std::future<R> fut = p->get_future();

    std::thread([p, f = std::move(f)]() mutable {
        try {
            // if constexpr picks one branch AT COMPILE TIME and discards the
            // other -- needed because promise<void>::set_value() takes no
            // argument, while every other set_value(v) does. A plain `if`
            // would have to compile both calls for every R, and set_value()
            // with no argument doesn't exist when R isn't void.
            if constexpr (std::is_void_v<R>) {
                f();
                p->set_value();
            } else {
                p->set_value(f());
            }
        } catch (...) {
            // current_exception() captures whatever exception is in flight
            // into a std::exception_ptr, which stays valid after this catch
            // block ends -- that's what lets it be stashed in the promise
            // and rethrown later, on a completely different thread, by
            // whichever thread calls future::get().
            p->set_exception(std::current_exception());
        }
    }).detach();

    return fut;
}
