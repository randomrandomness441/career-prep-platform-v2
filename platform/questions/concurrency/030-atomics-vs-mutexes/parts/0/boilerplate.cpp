// A counter, safe to increment concurrently from any number of threads. No
// increment may ever be lost.
class HotCounter {
public:
    HotCounter() noexcept = default;

    void increment() noexcept {
        // TODO: implement
    }

    long get() const noexcept {
        // TODO: implement
        return 0;
    }
};
