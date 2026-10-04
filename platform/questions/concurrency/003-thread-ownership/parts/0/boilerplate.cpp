#include <functional>
#include <thread>
#include <utility>

// A wrapper around std::thread that's safe to just let go out of scope --
// unlike std::thread itself, whose destructor calls std::terminate() if the
// thread is still joinable when it's destroyed.
class joining_thread {
    std::thread t;

public:
    joining_thread() noexcept = default;

    explicit joining_thread(std::function<void()> f) : t(std::move(f)) {}

    // TODO: what does the destructor need to do before t's own destructor runs?

    // TODO: make this movable (move constructor + move assignment), so it can
    //       live in a std::vector. Move assignment has to deal with whatever
    //       thread *this was already running before it takes on a new one.

    // TODO: make this non-copyable. std::thread itself can't be copied, so
    //       neither should this.

    bool joinable() const noexcept { return t.joinable(); }
    void join() { t.join(); }
    std::thread::id get_id() const noexcept { return t.get_id(); }
};
