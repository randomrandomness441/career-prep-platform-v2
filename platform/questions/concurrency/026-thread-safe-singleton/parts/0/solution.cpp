template <typename T>
class singleton {
public:
    singleton() = delete;
    singleton(const singleton&) = delete;
    singleton& operator=(const singleton&) = delete;

    static T& get() {
        // Meyers singleton. C++11 "magic statics": the compiler emits a guard
        // around this initialization. Exactly one thread runs the constructor;
        // the others wait; when they proceed, the constructor's writes are
        // visible to them (a release on the guard pairs with their acquire).
        // If the constructor throws, initialization is retried on the next call.
        static T instance;
        return instance;
    }
};
