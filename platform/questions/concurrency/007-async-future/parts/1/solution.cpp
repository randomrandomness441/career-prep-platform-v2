#include <future>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>

// packaged_task<R()> is a callable that, when invoked, runs the wrapped
// function and stuffs the answer -- or the exception -- into a shared state
// that a std::future reads. It is the thing std::async is built out of, with
// the "where does it run" decision handed back to you.
template <typename F, typename... Args>
auto spawn_task(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>> {
    using R = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;

    // Decay-copy the callable and the arguments into the task now, on the
    // caller's thread. Nothing the task touches lives in this stack frame,
    // so the caller may return the moment we detach.
    std::packaged_task<R()> task(
        [fn = std::forward<F>(f),
         bound = std::make_tuple(std::forward<Args>(args)...)]() mutable -> R {
            return std::apply(std::move(fn), std::move(bound));
        });

    // get_future() must be called before the task is moved away.
    std::future<R> result = task.get_future();

    // packaged_task is move-only, which is exactly what std::thread wants.
    // detach() is safe here because the task owns everything it uses, and the
    // future is how the caller observes completion.
    std::thread(std::move(task)).detach();

    return result;
}
