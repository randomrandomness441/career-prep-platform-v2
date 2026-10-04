### How it's called

```cpp
Logger logger(10000);

std::vector<std::thread> producers;
for (int id = 0; id < 8; ++id) {
    producers.emplace_back([&logger, id]{
        for (long seq = 0; seq < 500; ++seq) {
            bool ok = logger.log(id, seq, "request handled");   // never allocates
            // ok == false only once the logger is full
        }
    });
}

std::thread consumer([&logger]{
    for (int i = 0; i < 20; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        std::vector<LogRecord> batch = logger.drain();   // only fully-written records
        write_to_disk(batch);
    }
});

for (auto& t : producers) t.join();
consumer.join();
```

8 producer threads call `log()` concurrently; exactly one consumer thread calls `drain()`.
