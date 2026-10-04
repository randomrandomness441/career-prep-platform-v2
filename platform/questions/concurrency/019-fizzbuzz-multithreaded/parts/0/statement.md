# FizzBuzz Multithreaded

## ELI5: four kids playing FizzBuzz around a circle

Four kids are playing FizzBuzz together, counting from 1 to n as a group, but each kid
is only allowed to say certain things. One kid only ever says "fizz" (multiples of 3),
one only ever says "buzz" (multiples of 5), one only ever says "fizzbuzz" (multiples of
15), and the last kid says every plain number. They all know the rules and are itching to
jump in the moment it's their turn, but the count still has to come out in perfect order,
1 to n, with each kid only speaking on their own cue.

## What you're actually building

Four threads share one `FizzBuzz` object, constructed with a count `n`. Together they
must produce the FizzBuzz sequence for `1..n`, in order.

```cpp
class FizzBuzz {
public:
    FizzBuzz(int n);
    void fizz (std::function<void()> printFizz); // prints "fizz"
    void buzz (std::function<void()> printBuzz); // prints "buzz"
    void fizzbuzz(std::function<void()> printFizzBuzz); // prints "fizzbuzz"
    void number (std::function<void(int)> printNumber); // prints the integer
};
```

Each thread calls exactly one of the four methods, once, and that method loops internally:

- `fizz()` handles every value divisible by 3 but not 5.
- `buzz()` handles every value divisible by 5 but not 3.
- `fizzbuzz()` handles every value divisible by 15.
- `number()` handles everything else, printing the value itself.

For `n = 15` the output must be:

```
1 2 fizz 4 buzz fizz 7 8 fizz buzz 11 fizz 13 14 fizzbuzz
```

The four threads are started in an arbitrary order and the scheduler may run them in any
order.

## Why the constraints exist

- **No busy-waiting.** A kid with nothing to say right now has to actually sleep, not sit
 there checking "is it my turn?" in a loop.
- **All four calls must return** once the sequence is finished. A thread still asleep at
 the end is a failure, not a detail, the caller joins all four.
- **Works for any `n >= 0`.**
