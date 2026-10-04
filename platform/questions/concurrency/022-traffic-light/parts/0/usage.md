### How it's called

```cpp
traffic_light light;

std::vector<std::thread> cars;
for (int i = 0; i < 50; ++i) {
    int road = i % 2;   // alternating north/south and east/west arrivals
    cars.emplace_back([&light, road, i]{
        light.cross(road, [&]{ drive_across_bridge(i); });
    });
}
for (auto& t : cars) t.join();
```

Many cars from both axes call `cross` concurrently, in an interleaved arrival order; each
call blocks until it's safe for that car to go, runs `cross_car`, and returns once the car
is off the bridge.
