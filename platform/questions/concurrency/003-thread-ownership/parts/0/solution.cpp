#include <functional>
#include <thread>
#include <utility>

class joining_thread {
    std::thread t;

public:
    joining_thread() noexcept = default;

    explicit joining_thread(std::function<void()> f) : t(std::move(f)) {}

    // A moved-from object holds an empty handle, so the joinable() check is
    // required — join() on an empty handle throws.
    ~joining_thread() { if (t.joinable()) t.join(); }

    joining_thread(joining_thread&& other) noexcept : t(std::move(other.t)) {}

    joining_thread& operator=(joining_thread&& other) noexcept {
        if (this != &other) {              // self-move must not join then use
            // std::thread::operator= calls terminate() if *this is joinable,
            // so we have to finish our current thread before taking the new one.
            if (t.joinable()) t.join();
            t = std::move(other.t);
        }
        return *this;
    }

    joining_thread(const joining_thread&) = delete;
    joining_thread& operator=(const joining_thread&) = delete;

    bool joinable() const noexcept { return t.joinable(); }
    void join() { t.join(); }
    std::thread::id get_id() const noexcept { return t.get_id(); }
};
