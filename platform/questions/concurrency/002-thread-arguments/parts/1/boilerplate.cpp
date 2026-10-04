#include <semaphore>
#include <string>
#include <thread>

struct Result { std::string text; };

std::thread make_labeler(Result* out, std::binary_semaphore* go, int id) {
    std::string label = "worker-" + std::to_string(id);   // a local

    // TODO: this captures `label` by reference. By the time the thread runs,
    //       make_labeler has returned and that storage belongs to nobody.
    return std::thread([&label, out, go] {
        go->acquire();
        out->text = label;
    });
}
