#include <semaphore>
#include <string>
#include <thread>

struct Result { std::string text; };

std::thread make_labeler(Result* out, std::binary_semaphore* go, int id) {
    std::string label = "worker-" + std::to_string(id);

    // Capture by move: the closure now OWNS the string, and the closure lives
    // inside the std::thread object, so it survives exactly as long as the
    // thread does. `out` and `go` stay pointers because the caller guarantees
    // they outlive the thread — that is the contract in the signature.
    return std::thread([label = std::move(label), out, go] {
        go->acquire();
        out->text = label;
    });
}
