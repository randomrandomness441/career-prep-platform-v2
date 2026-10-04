# Traffic Light Controlled Intersection

## ELI5: one bridge, two directions, a light that has to be fair

Picture a single-lane bridge crossing a river, with roads meeting it from two directions:
north/south and east/west. Cars going the same axis, say several north/south cars, can
be on the bridge together, crossing side by side. Cars from *different* axes must never
be on the bridge at the same time. That's a head-on collision waiting to happen.

The naive fix is "just let cars through one at a time, whoever's next." It technically
works, but it turns the bridge into a single-file stop sign, not a real traffic light
where a whole stream of green-light traffic flows through together. The harder, real
version is that green-axis cars keep flowing *as a group*, but the light still has to
notice when the other axis has been waiting and actually switch. Otherwise a steady
stream of north/south traffic can hog the bridge forever and east/west never gets a
turn. LeetCode 1279 skips that last part, and most published answers to it get exactly
that bug wrong.

## What you're actually building

```cpp
class traffic_light {
public:
    // road 0 = north/south axis, road 1 = east/west axis.
    // Blocks until this car's axis has the light AND the axis is open to
    // newcomers, then invokes cross_car() for the crossing. Returns once
    // the car is off the bridge.
    void cross(int road, std::function<void()> cross_car);
};
```

## Requirements

1. **Safety.** Two cars from different axes are never on the bridge at the same instant.
   "On the bridge" means inside the `cross_car` callback.
2. **Liveness.** Every car crosses. Under a mixed, continuous load both axes keep making
   progress. Nothing deadlocks and nobody waits forever.
3. **Same-axis concurrency.** The whole point of a light, versus a stop sign, is that a
   queue of cars on one axis crosses *together*. Several cars on the green axis must be
   able to run `cross_car` at the same time. This rules out "just hold one mutex for the
   whole crossing," because that's a stop sign, not a traffic light.
4. **The axis closes fairly.** The light switches axis when the bridge *empties*, and a
   green axis closes to newcomers as soon as a car from the other axis is waiting. A car
   that arrives after that waits, even though the light is technically still green for
   its axis. This is what stops an endless north/south stream from starving east/west
   forever.

## Why the constraints exist

- **`std::mutex` + `std::condition_variable`, and whatever plain state you need.** No
  atomics for the intersection state, no sleeps, no polling. The coordination has to be
  real, not a busy loop dressed up.
- **Wait with the predicate overload, `cv.wait(lk, pred)`, never bare `wait`.** Same
  reasoning as always. The bare form can't tell "woken for a real reason" from "woken for
  nothing."

Ask yourself before writing: *which line of my code actually changes the light, and what
guarantees that line is ever reached?* The published answer this question is based on got
exactly that wrong.
