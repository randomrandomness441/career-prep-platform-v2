// Return a reference to the single, shared T instance. Safe to call
// concurrently from any number of threads: no two callers may ever see two
// different instances, and none may observe a partially constructed one.
template <typename T>
class singleton {
public:
    static T& get() {
        // TODO: implement
        static T* dummy = nullptr;
        return *dummy;
    }
};
