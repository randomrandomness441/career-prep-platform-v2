#include <condition_variable>
#include <functional>
#include <mutex>

// A four-way intersection with one bridge. Two axes: road 0 is north/south,
// road 1 is east/west. Cars on the green axis may cross together; the other
// axis waits. The light must actually switch once the bridge empties and
// cars are waiting on the other axis.
class traffic_light {
    std::mutex m_;
    std::condition_variable cv_;

public:
    // Blocks until this car's axis has the light, then runs cross_car()
    // while the car crosses.
    void cross(int road, std::function<void()> cross_car) {
        // TODO: implement
        (void)road;
        cross_car();
    }
};
