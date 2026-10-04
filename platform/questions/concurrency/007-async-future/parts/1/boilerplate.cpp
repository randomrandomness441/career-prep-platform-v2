#include <future>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>

// Hands back a real std::future holding the right value, computed on the
// caller's thread, before returning -- not parallel yet.
//
// The template signature below is the price of "works for any callable, any
// arguments, including move-only ones" -- not a concurrency concept. See the
// reading's "template machinery in spawn_task" glossary if any piece of it
// (F&&, std::forward, invoke_result_t, decay_t, if constexpr) is unfamiliar.

template <typename F, typename... Args>
auto spawn_task(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>> {
    using R = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;

    std::promise<R> p;
    std::future<R> result = p.get_future();

    // TODO 1: this runs right here, on the calling thread. Move it onto a
    //         thread of its own.
    // TODO 2: if f throws, set_value is never reached, the promise is
    //         destroyed unfulfilled, and get() throws future_error
    //         (broken_promise) instead of the real exception. Worse: once
    //         this runs on its own thread, an escaping exception calls
    //         std::terminate. Find the type that handles both for you.
    // TODO 3: whatever runs the task must not require the caller to join
    //         anything -- the future is the only handle the caller gets.
    if constexpr (std::is_void_v<R>) {
        std::apply(std::forward<F>(f), std::forward_as_tuple(std::forward<Args>(args)...));
        p.set_value();
    } else {
        p.set_value(std::apply(std::forward<F>(f),
                               std::forward_as_tuple(std::forward<Args>(args)...)));
    }

    return result;
}
